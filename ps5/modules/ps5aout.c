/*
 * ps5aout: the app's audio output. PCM goes to the player's amem callbacks,
 * exactly as VLC's amem module does it (player.cc's audio thread plays it).
 * Dolby and DTS can instead go out untouched for the soundbar or receiver to
 * decode (passthrough), when the player allows it ("ps5-passthrough") and the
 * sink's EDID lists the format.
 *
 * Passthrough: Start() answers VLC's compressed format with S/PDIF at the
 * console port's rate and layout, so VLC puts its tospdif converter in front:
 * what Play() gets is whole IEC 61937 bursts with their play time. A writer
 * thread sends them a port grain at a time when they're due (null data before,
 * dropped whole if late), so lip sync works as for PCM. "ps5-passthrough-delay"
 * (ms) sends them that much earlier, for the sink's own decoding delay.
 * Anything refused here (codec, sink, console) makes VLC decode the file.
 *
 * The console side, and when the HDMI mode changes, is platform_ps5.c.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#define MODULE_NAME ps5aout
#define MODULE_STRING "ps5aout"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>

#include "passthrough.h"
#include "platform.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname("PS5")
    set_description("PS5 audio output, with HDMI passthrough")
    set_capability("audio output", 0)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Open, Close)
    add_bool("ps5-passthrough", false, "HDMI passthrough", "Dolby and DTS to the receiver", false)
        change_private()
    add_integer("ps5-passthrough-delay", 0, "Passthrough delay", "Receiver decoding delay (ms)", false)
        change_integer_range(0, 500)
        change_private()
vlc_module_end()

struct aout_sys_t {
    /* PCM: the player's amem callbacks (libvlc_audio_set_callbacks). */
    void *opaque;
    void *setup_opaque;
    int (*setup)(void **, char *, unsigned *, unsigned *);
    void (*cleanup)(void *opaque);
    void (*play)(void *opaque, const void *data, unsigned count, int64_t pts);
    void (*pause)(void *opaque, int64_t pts);
    void (*resume)(void *opaque, int64_t pts);
    void (*flush)(void *opaque);
    void (*drain)(void *opaque);
    int (*set_volume)(void *opaque, float vol, bool mute);
    float volume;
    bool mute;
    bool ready;

    /* Passthrough, between Start and Stop. */
    bool bitstream;
    enum pt_kind kind;
    struct pt_carrier carrier;
    size_t grain_bytes;
    mtime_t sink_delay;
    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t drained;
    block_t *head, **tail; /* bursts not sent yet */
    size_t head_pos;       /* bytes of head already sent */
    bool paused, quit;
};

/* ---- passthrough ------------------------------------------------------------ */

static void ClearQueue(aout_sys_t *sys)
{
    block_ChainRelease(sys->head);
    sys->head = NULL;
    sys->tail = &sys->head;
    sys->head_pos = 0;
}

static void PopHead(aout_sys_t *sys)
{
    block_t *b = sys->head;
    sys->head = b->p_next;
    if (!sys->head)
        sys->tail = &sys->head;
    sys->head_pos = 0;
    block_Release(b);
}

static void *Writer(void *data)
{
    audio_output_t *aout = data;
    aout_sys_t *sys = aout->sys;
    const struct pt_carrier c = sys->carrier;
    const size_t fb = c.frame_bytes;
    /* As player.cc counts it: the grain playing plus the one queued. */
    const mtime_t latency = 2 * (mtime_t)c.grain_frames * CLOCK_FREQ / c.rate;
    uint8_t *grain = malloc(sys->grain_bytes);
    if (!grain)
        return NULL;
    for (;;) {
        size_t filled = 0;
        vlc_mutex_lock(&sys->lock);
        if (sys->quit) {
            vlc_mutex_unlock(&sys->lock);
            break;
        }
        const mtime_t heard = mdate() + latency + sys->sink_delay;
        while (!sys->paused && filled < sys->grain_bytes && sys->head) {
            block_t *b = sys->head;
            if (sys->head_pos == 0) {
                mtime_t slot = heard + (mtime_t)(filled / fb) * CLOCK_FREQ / c.rate;
                enum pt_slot s = pt_schedule(b->i_pts, slot);
                if (s == PT_EARLY)
                    break;
                if (s == PT_LATE) {
                    PopHead(sys);
                    continue;
                }
            }
            size_t n = b->i_buffer - sys->head_pos;
            if (n > sys->grain_bytes - filled)
                n = sys->grain_bytes - filled;
            memcpy(grain + filled, b->p_buffer + sys->head_pos, n);
            filled += n;
            sys->head_pos += n;
            if (sys->head_pos >= b->i_buffer)
                PopHead(sys);
        }
        if (!sys->head)
            vlc_cond_broadcast(&sys->drained);
        vlc_mutex_unlock(&sys->lock);
        /* Nothing due: null data keeps the sink locked on. Part of a grain:
         * zeros after the bursts, which IEC 61937 allows. */
        if (filled == 0)
            pt_null_grain(grain, sys->grain_bytes);
        else
            memset(grain + filled, 0, sys->grain_bytes - filled);
        plat_bitstream_output(grain);
    }
    free(grain);
    return NULL;
}

static int StartBitstream(audio_output_t *aout, audio_sample_format_t *fmt)
{
    aout_sys_t *sys = aout->sys;
    if (!var_InheritBool(aout, "ps5-passthrough"))
        return VLC_EGENERIC;

    enum pt_codec codec;
    switch (fmt->i_format) {
    case VLC_CODEC_A52: codec = PT_CODEC_AC3; break;
    case VLC_CODEC_EAC3: codec = PT_CODEC_EAC3; break;
    case VLC_CODEC_DTS:
        /* The decoder sets "dtshd" for a stream with DTS-HD extensions. */
        codec = var_Type(aout, "dtshd") && var_GetBool(aout, "dtshd") ? PT_CODEC_DTSHD : PT_CODEC_DTS;
        break;
    case VLC_CODEC_TRUEHD:
    case VLC_CODEC_MLP: codec = PT_CODEC_TRUEHD; break;
    default: return VLC_EGENERIC;
    }
    /* The ports run at 48 kHz multiples: 44.1 kHz streams are decoded. */
    if (fmt->i_rate == 0 || fmt->i_rate % 48000 != 0)
        return VLC_EGENERIC;

    struct pt_sink sink;
    if (!plat_bitstream_sink(&sink))
        return VLC_EGENERIC;
    enum pt_kind kind = pt_kind_for(codec, &sink);
    if (kind == PT_NONE) {
        msg_Dbg(aout, "passthrough: the sink doesn't list %4.4s", (const char *)&fmt->i_format);
        return VLC_EGENERIC;
    }
    if (!plat_bitstream_start(kind)) {
        msg_Warn(aout, "passthrough: the console refused %s", pt_kind_name(kind));
        return VLC_EGENERIC;
    }

    sys->kind = kind;
    sys->carrier = pt_carrier_for(kind);
    sys->grain_bytes = pt_grain_bytes(kind);
    sys->sink_delay = var_InheritInteger(aout, "ps5-passthrough-delay") * 1000;
    sys->head = NULL;
    sys->tail = &sys->head;
    sys->head_pos = 0;
    sys->paused = false;
    sys->quit = false;
    if (vlc_clone(&sys->thread, Writer, aout, VLC_THREAD_PRIORITY_OUTPUT)) {
        plat_bitstream_stop();
        return VLC_ENOMEM;
    }
    sys->bitstream = true;

    /* What tospdif makes and the port takes: 16-bit stereo frames (8-channel
     * for TrueHD's MAT) at the carrier's rate. Above 48 kHz, tospdif packs
     * DTS as DTS-HD. */
    fmt->i_format = VLC_CODEC_SPDIFL;
    fmt->i_rate = sys->carrier.rate;
    fmt->i_physical_channels = sys->carrier.channels == 8 ? AOUT_CHANS_7_1 : AOUT_CHANS_STEREO;
    fmt->i_channels = sys->carrier.channels;
    fmt->i_bytes_per_frame = sys->carrier.frame_bytes;
    fmt->i_frame_length = 1;
    msg_Info(aout, "passthrough: %s to the sink", pt_kind_name(kind));
    return VLC_SUCCESS;
}

static void StopBitstream(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    vlc_mutex_lock(&sys->lock);
    sys->quit = true;
    vlc_mutex_unlock(&sys->lock);
    vlc_join(sys->thread, NULL);
    ClearQueue(sys);
    sys->bitstream = false;
    /* The HDMI mode stays for the next file (platform_ps5.c). */
    plat_bitstream_stop();
}

/* ---- the amem path, and both ---------------------------------------------- */

static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    if (sys->bitstream) {
        block->p_next = NULL;
        vlc_mutex_lock(&sys->lock);
        *sys->tail = block;
        sys->tail = &block->p_next;
        vlc_mutex_unlock(&sys->lock);
        return;
    }
    sys->play(sys->opaque, block->p_buffer, block->i_nb_samples, block->i_pts);
    block_Release(block);
}

static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    if (sys->bitstream) {
        vlc_mutex_lock(&sys->lock);
        sys->paused = paused;
        vlc_mutex_unlock(&sys->lock);
        return;
    }
    void (*cb)(void *, int64_t) = paused ? sys->pause : sys->resume;
    if (cb)
        cb(sys->opaque, date);
}

static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    if (sys->bitstream) {
        vlc_mutex_lock(&sys->lock);
        if (wait) {
            /* What's queued plays out (at most a few seconds). */
            mtime_t until = mdate() + 3 * CLOCK_FREQ;
            while (sys->head && !sys->paused)
                if (vlc_cond_timedwait(&sys->drained, &sys->lock, until))
                    break;
        }
        ClearQueue(sys);
        vlc_mutex_unlock(&sys->lock);
        return;
    }
    void (*cb)(void *) = wait ? sys->drain : sys->flush;
    if (cb)
        cb(sys->opaque);
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    sys->volume = vol;
    if (!sys->ready)
        return 0;
    return sys->set_volume(sys->opaque, vol, sys->mute) ? -1 : 0;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    sys->mute = mute;
    if (!sys->ready)
        return 0;
    return sys->set_volume(sys->opaque, sys->volume, mute) ? -1 : 0;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    if (sys->bitstream) {
        StopBitstream(aout);
        return;
    }
    if (sys->cleanup)
        sys->cleanup(sys->opaque);
    sys->ready = false;
}

static int StartPcm(audio_output_t *aout, audio_sample_format_t *fmt)
{
    aout_sys_t *sys = aout->sys;
    char format[5] = "S16N";
    unsigned channels = aout_FormatNbChannels(fmt);
    if (channels == 0)
        return VLC_EGENERIC;
    /* PCM is muted while HDMI carries a bitstream: normal output first. */
    if (plat_bitstream_on() && !plat_bitstream_reset())
        msg_Warn(aout, "passthrough still writing: PCM may be muted");

    sys->opaque = sys->setup_opaque;
    if (sys->setup && sys->setup(&sys->opaque, format, &fmt->i_rate, &channels))
        return VLC_EGENERIC;
    sys->ready = true;
    if (sys->set_volume)
        sys->set_volume(sys->opaque, sys->volume, sys->mute);
    if (fmt->i_rate == 0 || channels == 0 || channels > 8 || strcmp(format, "S16N")) {
        msg_Err(aout, "format not supported: %s, %u channel(s), %u Hz", format, channels, fmt->i_rate);
        Stop(aout);
        return VLC_EGENERIC;
    }
    /* The channel counts player.cc's amem_setup answers with. */
    switch (channels) {
    case 1: fmt->i_physical_channels = AOUT_CHAN_CENTER; break;
    case 2: fmt->i_physical_channels = AOUT_CHANS_2_0; break;
    case 6: fmt->i_physical_channels = AOUT_CHANS_5_1; break;
    case 8: fmt->i_physical_channels = AOUT_CHANS_7_1; break;
    default:
        msg_Err(aout, "%u channels not supported", channels);
        Stop(aout);
        return VLC_EGENERIC;
    }
    fmt->i_format = VLC_CODEC_S16N;
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    return VLC_SUCCESS;
}

static int Start(audio_output_t *aout, audio_sample_format_t *fmt)
{
    if (AOUT_FMT_LINEAR(fmt))
        return StartPcm(aout, fmt);
    return StartBitstream(aout, fmt);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->setup_opaque = var_InheritAddress(obj, "amem-data");
    sys->setup = var_InheritAddress(obj, "amem-setup");
    sys->cleanup = var_InheritAddress(obj, "amem-cleanup");
    sys->play = var_InheritAddress(obj, "amem-play");
    sys->pause = var_InheritAddress(obj, "amem-pause");
    sys->resume = var_InheritAddress(obj, "amem-resume");
    sys->flush = var_InheritAddress(obj, "amem-flush");
    sys->drain = var_InheritAddress(obj, "amem-drain");
    sys->set_volume = var_InheritAddress(obj, "amem-set-volume");
    sys->volume = 1.f;
    /* player.cc sets all of these; without them there's nothing to play to. */
    if (!sys->play || !sys->setup || !sys->set_volume) {
        free(sys);
        return VLC_EGENERIC;
    }
    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->drained);
    sys->tail = &sys->head;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = NULL;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;
    vlc_cond_destroy(&sys->drained);
    vlc_mutex_destroy(&sys->lock);
    free(sys);
}
