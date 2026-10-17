/*
 * HDMI passthrough: the soundbar or receiver decodes Dolby and DTS, the
 * console only carries the bitstream (IEC 61937 bursts, which VLC's tospdif
 * converter packs). This file holds the decisions, with no console or VLC
 * calls, so tests/passthrough_test.c checks them on a PC:
 *
 *   - what the sink can decode, from the console's copy of its EDID;
 *   - which bitstream a file's codec goes out as (or none: decoded to PCM);
 *   - the console port each bitstream needs;
 *   - when the HDMI output has to change mode (sticky: as rarely as possible,
 *     since receivers get confused by back-to-back changes);
 *   - whether a block of bursts is early, due or too late to send.
 *
 * Console facts (FW 12.70) are from github.com/sainsaji/PS5-Audio-Passthrough-Research.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What a file's audio is, as far as passthrough cares. */
enum pt_codec {
    PT_CODEC_AC3,
    PT_CODEC_EAC3,   /* Dolby Digital Plus, Atmos (JOC) included */
    PT_CODEC_DTS,    /* DTS core only */
    PT_CODEC_DTSHD,  /* DTS core plus DTS-HD extensions (HRA, MA) */
    PT_CODEC_TRUEHD, /* Dolby TrueHD, Atmos included; MLP the same */
};

/* A bitstream on HDMI: one console output mode. */
enum pt_kind {
    PT_NONE = -1, /* not a bitstream: normal PCM output */
    PT_AC3,
    PT_EAC3,
    PT_DTS,
    PT_DTSHD,
    PT_TRUEHD,
    PT_KINDS
};

/* CEA-861 audio coding types, as the EDID lists them. */
enum {
    PT_CEA_AC3 = 2,
    PT_CEA_DTS = 7,
    PT_CEA_EAC3 = 10,
    PT_CEA_DTSHD = 11,
    PT_CEA_MAT = 12, /* MLP / TrueHD */
};

#define PT_SINK_MAX 16
/* The audio formats the TV or receiver says it decodes. */
struct pt_sink {
    int count;
    uint8_t coding[PT_SINK_MAX];   /* CEA-861 coding type */
    uint8_t channels[PT_SINK_MAX];
};

/* sceAudioOutSysGetHdmiMonitorInfo's 0x180 bytes: the number of short audio
 * descriptors as a 32-bit value at +0x94, then 8-byte entries from +0x98
 * (coding type, channels, ...). False if the buffer is too short. */
#define PT_MONITOR_INFO_SIZE 0x180
bool pt_sink_parse(const uint8_t *info, size_t size, struct pt_sink *out);
bool pt_sink_has(const struct pt_sink *sink, int cea_coding);

/* The bitstream a codec goes out as on this sink, or PT_NONE (decode it).
 * DTS-HD the sink can't take still goes out as its DTS core. */
enum pt_kind pt_kind_for(enum pt_codec codec, const struct pt_sink *sink);

/* The console port and HDMI mode for a bitstream. Frames are what the port
 * and VLC count: 16-bit stereo (4 bytes) or 16-bit 8-channel (16 bytes).
 * TrueHD's port is opened as "768 kHz" but takes its 16-byte frames at
 * 192 kHz, so rate is 192000 there. */
struct pt_carrier {
    bool sys;              /* sceAudioOutSys* (TrueHD), else sceAudioOutEx* */
    int mode;              /* the Open and ConfigureOutput mode */
    unsigned grain_frames; /* frames per sceAudioOutOutput call */
    unsigned rate;         /* frames per second */
    unsigned frame_bytes;
    unsigned channels;     /* what VLC is told: 2, or 8 for TrueHD */
};
struct pt_carrier pt_carrier_for(enum pt_kind kind);
size_t pt_grain_bytes(enum pt_kind kind);
const char *pt_kind_name(enum pt_kind kind);

/* What the HDMI output has to do to go from `current` to `wanted` (PT_NONE
 * for PCM). Sticky: the same bitstream again keeps the mode as it is. */
enum pt_step {
    PT_STAY,   /* nothing to do */
    PT_OPEN,   /* normal output to a bitstream */
    PT_SWITCH, /* one bitstream to another: reset, then open */
    PT_RESET,  /* a bitstream back to normal PCM output */
};
enum pt_step pt_plan(enum pt_kind current, enum pt_kind wanted);

/* A block of bursts due at `due_us`, against the time the next frame will
 * be heard (`slot_us`): more than 15 ms early waits (null data in between),
 * more than 60 ms late is dropped whole (a burst can't be cut), else sent. */
enum pt_slot { PT_EARLY, PT_DUE, PT_LATE };
enum pt_slot pt_schedule(int64_t due_us, int64_t slot_us);

/* One grain of IEC 61937 null data: preamble Pa Pb, data type 0, zeros. It
 * keeps the receiver locked on the bitstream while nothing is due. */
void pt_null_grain(uint8_t *grain, size_t bytes);

#ifdef __cplusplus
}
#endif
