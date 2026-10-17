/*
 * HDMI passthrough decisions (see passthrough.h).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "passthrough.h"

#include <string.h>

bool pt_sink_parse(const uint8_t *info, size_t size, struct pt_sink *out)
{
    memset(out, 0, sizeof(*out));
    if (!info || size < PT_INFO_SADS_AT)
        return false;
    uint32_t n;
    memcpy(&n, info + PT_INFO_COUNT_AT, sizeof(n));
    for (uint32_t i = 0; i < n && out->count < PT_SINK_MAX; i++) {
        size_t at = PT_INFO_SADS_AT + (size_t)i * 8;
        if (at + 8 > size)
            break;
        /* A coding type of 0 or above 15 isn't a descriptor: the list ended. */
        if (info[at] == 0 || info[at] > 15)
            break;
        out->coding[out->count] = info[at];
        out->channels[out->count] = info[at + 1];
        out->count++;
    }
    return true;
}

bool pt_sink_has(const struct pt_sink *sink, int cea_coding)
{
    if (!sink)
        return false;
    for (int i = 0; i < sink->count; i++)
        if (sink->coding[i] == cea_coding)
            return true;
    return false;
}

enum pt_kind pt_kind_for(enum pt_codec codec, const struct pt_sink *sink)
{
    switch (codec) {
    case PT_CODEC_AC3:
        return pt_sink_has(sink, PT_CEA_AC3) ? PT_AC3 : PT_NONE;
    case PT_CODEC_EAC3:
        return pt_sink_has(sink, PT_CEA_EAC3) ? PT_EAC3 : PT_NONE;
    case PT_CODEC_DTSHD:
        if (pt_sink_has(sink, PT_CEA_DTSHD))
            return PT_DTSHD;
        /* Its core, for a DTS-only sink. */
        return pt_sink_has(sink, PT_CEA_DTS) ? PT_DTS : PT_NONE;
    case PT_CODEC_DTS:
        return pt_sink_has(sink, PT_CEA_DTS) ? PT_DTS : PT_NONE;
    case PT_CODEC_TRUEHD:
        return pt_sink_has(sink, PT_CEA_MAT) ? PT_TRUEHD : PT_NONE;
    }
    return PT_NONE;
}

/* FINDINGS.md items 12-17: ExOpen gives 256 frames at 48 kHz for modes 0-2
 * and 1024 at 192 kHz for 3, 4 and 10, always 16-bit stereo; SysOpen mode 5
 * is the Blu-ray player's TrueHD port, 16-bit 8-channel. */
static const struct pt_carrier carriers[PT_KINDS] = {
    [PT_AC3] = { false, 0, 256, 48000, 4, 2 },
    [PT_EAC3] = { false, 3, 1024, 192000, 4, 2 },
    [PT_DTS] = { false, 2, 256, 48000, 4, 2 },
    [PT_DTSHD] = { false, 4, 1024, 192000, 4, 2 },
    [PT_TRUEHD] = { true, 5, 1024, 192000, 16, 8 },
};

struct pt_carrier pt_carrier_for(enum pt_kind kind)
{
    if (kind < 0 || kind >= PT_KINDS) {
        struct pt_carrier none = { false, -1, 0, 0, 0, 0 };
        return none;
    }
    return carriers[kind];
}

size_t pt_grain_bytes(enum pt_kind kind)
{
    struct pt_carrier c = pt_carrier_for(kind);
    return (size_t)c.grain_frames * c.frame_bytes;
}

const char *pt_kind_name(enum pt_kind kind)
{
    switch (kind) {
    case PT_AC3: return "Dolby Digital";
    case PT_EAC3: return "Dolby Digital Plus";
    case PT_DTS: return "DTS";
    case PT_DTSHD: return "DTS-HD";
    case PT_TRUEHD: return "Dolby TrueHD";
    default: return "?";
    }
}

enum pt_step pt_plan(enum pt_kind current, enum pt_kind wanted)
{
    if (current == wanted)
        return PT_STAY;
    if (current == PT_NONE)
        return PT_OPEN;
    if (wanted == PT_NONE)
        return PT_RESET;
    return PT_SWITCH;
}

enum pt_slot pt_schedule(int64_t due_us, int64_t slot_us)
{
    if (due_us > slot_us + 15000)
        return PT_EARLY;
    if (due_us < slot_us - 60000)
        return PT_LATE;
    return PT_DUE;
}

void pt_null_grain(uint8_t *grain, size_t bytes)
{
    memset(grain, 0, bytes);
    if (bytes < 8)
        return;
    grain[0] = 0x72; /* Pa 0xF872 */
    grain[1] = 0xF8;
    grain[2] = 0x1F; /* Pb 0x4E1F */
    grain[3] = 0x4E;
}
