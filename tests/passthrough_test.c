/*
 * Tests for src/passthrough.c, on a PC:
 *
 *   cc -std=c11 -Wall -Wextra -Isrc tests/passthrough_test.c src/passthrough.c -o /tmp/pt && /tmp/pt
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "passthrough.h"

#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        }                                                                      \
    } while (0)

/* A monitor-info buffer with these (coding, channels) descriptors. */
static void make_info(uint8_t *info, const uint8_t (*sad)[2], uint32_t n)
{
    memset(info, 0, PT_MONITOR_INFO_SIZE);
    memcpy(info + PT_INFO_NAME_AT, "LG ULTRAGEAR", 12);
    memcpy(info + PT_INFO_COUNT_AT, &n, sizeof(n));
    for (uint32_t i = 0; i < n; i++) {
        info[PT_INFO_SADS_AT + i * 8] = sad[i][0];
        info[PT_INFO_SADS_AT + i * 8 + 1] = sad[i][1];
    }
}

/* The research repo's LG UltraGear soundbar chain: everything. */
static const uint8_t full_sads[9][2] = {
    { 1, 2 }, { 1, 6 }, { 12, 8 }, { 10, 8 }, { 2, 6 }, { 6, 6 }, { 7, 6 }, { 11, 8 }, { 11, 8 },
};

static void test_sink_parse(void)
{
    uint8_t info[PT_MONITOR_INFO_SIZE];
    struct pt_sink sink;
    make_info(info, full_sads, 9);
    CHECK(pt_sink_parse(info, sizeof(info), &sink));
    CHECK(sink.count == 9);
    CHECK(sink.coding[2] == PT_CEA_MAT && sink.channels[2] == 8);
    CHECK(pt_sink_has(&sink, PT_CEA_AC3));
    CHECK(pt_sink_has(&sink, PT_CEA_DTSHD));
    CHECK(!pt_sink_has(&sink, 9 /* ATRAC */));

    /* Too short a buffer: nothing. */
    CHECK(!pt_sink_parse(info, PT_INFO_SADS_AT - 1, &sink));

    /* A count that runs past the buffer stops at the last whole entry. */
    uint32_t big = 1000;
    memcpy(info + PT_INFO_COUNT_AT, &big, sizeof(big));
    CHECK(pt_sink_parse(info, sizeof(info), &sink));
    CHECK(sink.count == 9); /* entries after the ninth are zero: the list ends */

    /* An empty list (PCM-only TV or no HDMI): no formats. */
    make_info(info, full_sads, 0);
    CHECK(pt_sink_parse(info, sizeof(info), &sink));
    CHECK(sink.count == 0);
    CHECK(!pt_sink_has(&sink, PT_CEA_AC3));

    /* More descriptors than we keep: the first PT_SINK_MAX. */
    uint8_t many[20][2];
    for (int i = 0; i < 20; i++) {
        many[i][0] = 2;
        many[i][1] = 6;
    }
    make_info(info, (const uint8_t(*)[2])many, 20);
    CHECK(pt_sink_parse(info, sizeof(info), &sink));
    CHECK(sink.count == PT_SINK_MAX);
}

static void test_kind_for(void)
{
    uint8_t info[PT_MONITOR_INFO_SIZE];
    struct pt_sink full, dolby_only, none;
    make_info(info, full_sads, 9);
    pt_sink_parse(info, sizeof(info), &full);
    static const uint8_t dolby[3][2] = { { 1, 2 }, { 2, 6 }, { 10, 8 } };
    make_info(info, dolby, 3);
    pt_sink_parse(info, sizeof(info), &dolby_only);
    make_info(info, full_sads, 0);
    pt_sink_parse(info, sizeof(info), &none);

    CHECK(pt_kind_for(PT_CODEC_AC3, &full) == PT_AC3);
    CHECK(pt_kind_for(PT_CODEC_EAC3, &full) == PT_EAC3);
    CHECK(pt_kind_for(PT_CODEC_DTS, &full) == PT_DTS);
    CHECK(pt_kind_for(PT_CODEC_DTSHD, &full) == PT_DTSHD);
    CHECK(pt_kind_for(PT_CODEC_TRUEHD, &full) == PT_TRUEHD);

    /* Only what the sink lists; the rest is decoded. */
    CHECK(pt_kind_for(PT_CODEC_AC3, &dolby_only) == PT_AC3);
    CHECK(pt_kind_for(PT_CODEC_EAC3, &dolby_only) == PT_EAC3);
    CHECK(pt_kind_for(PT_CODEC_TRUEHD, &dolby_only) == PT_NONE);
    CHECK(pt_kind_for(PT_CODEC_DTS, &dolby_only) == PT_NONE);
    CHECK(pt_kind_for(PT_CODEC_AC3, &none) == PT_NONE);
    CHECK(pt_kind_for(PT_CODEC_AC3, NULL) == PT_NONE);

    /* DTS-HD on a DTS-core-only sink: its core. */
    static const uint8_t dts_core[2][2] = { { 1, 2 }, { 7, 6 } };
    struct pt_sink core;
    make_info(info, dts_core, 2);
    pt_sink_parse(info, sizeof(info), &core);
    CHECK(pt_kind_for(PT_CODEC_DTSHD, &core) == PT_DTS);
}

static void test_carriers(void)
{
    /* The verified table: Ex modes 0, 2, 3, 4; TrueHD on Sys mode 5. */
    struct pt_carrier c = pt_carrier_for(PT_AC3);
    CHECK(!c.sys_api && c.mode == 0 && c.grain_frames == 256 && c.rate == 48000 && c.frame_bytes == 4 &&
          c.channels == 2);
    c = pt_carrier_for(PT_DTS);
    CHECK(!c.sys_api && c.mode == 2 && c.grain_frames == 256 && c.rate == 48000);
    c = pt_carrier_for(PT_EAC3);
    CHECK(!c.sys_api && c.mode == 3 && c.grain_frames == 1024 && c.rate == 192000 && c.frame_bytes == 4);
    c = pt_carrier_for(PT_DTSHD);
    CHECK(!c.sys_api && c.mode == 4 && c.grain_frames == 1024 && c.rate == 192000);
    c = pt_carrier_for(PT_TRUEHD);
    CHECK(c.sys_api && c.mode == 5 && c.grain_frames == 1024 && c.rate == 192000 && c.frame_bytes == 16 &&
          c.channels == 8);

    CHECK(pt_grain_bytes(PT_AC3) == 1024);
    CHECK(pt_grain_bytes(PT_EAC3) == 4096);
    CHECK(pt_grain_bytes(PT_TRUEHD) == 16384);
    CHECK(pt_grain_bytes(PT_NONE) == 0);

    /* A grain lasts the same whether it's measured in frames or bytes. */
    for (int k = 0; k < PT_KINDS; k++) {
        c = pt_carrier_for((enum pt_kind)k);
        CHECK(pt_grain_bytes((enum pt_kind)k) == (size_t)c.grain_frames * c.frame_bytes);
        CHECK(strcmp(pt_kind_name((enum pt_kind)k), "?") != 0);
    }
}

static void test_plan(void)
{
    CHECK(pt_plan(PT_NONE, PT_NONE) == PT_STAY);
    CHECK(pt_plan(PT_NONE, PT_AC3) == PT_OPEN);
    CHECK(pt_plan(PT_AC3, PT_AC3) == PT_STAY);     /* sticky: file after file */
    CHECK(pt_plan(PT_TRUEHD, PT_TRUEHD) == PT_STAY);
    CHECK(pt_plan(PT_AC3, PT_EAC3) == PT_SWITCH);
    CHECK(pt_plan(PT_DTSHD, PT_DTS) == PT_SWITCH);
    CHECK(pt_plan(PT_EAC3, PT_NONE) == PT_RESET);  /* PCM can't play in bitstream mode */
}

static void test_schedule(void)
{
    CHECK(pt_schedule(1000000, 1000000) == PT_DUE);
    CHECK(pt_schedule(1015000, 1000000) == PT_DUE);   /* 15 ms early: close enough */
    CHECK(pt_schedule(1015001, 1000000) == PT_EARLY);
    CHECK(pt_schedule(940000, 1000000) == PT_DUE);    /* 60 ms late: still sent */
    CHECK(pt_schedule(939999, 1000000) == PT_LATE);
}

static void test_null_grain(void)
{
    uint8_t g[1024];
    memset(g, 0xAA, sizeof(g));
    pt_null_grain(g, sizeof(g));
    /* Pa 0xF872, Pb 0x4E1F, Pc 0 (null data), Pd 0, as little-endian words. */
    static const uint8_t head[8] = { 0x72, 0xF8, 0x1F, 0x4E, 0, 0, 0, 0 };
    CHECK(memcmp(g, head, 8) == 0);
    int nonzero = 0;
    for (size_t i = 8; i < sizeof(g); i++)
        nonzero |= g[i];
    CHECK(nonzero == 0);
    uint8_t tiny[4] = { 9, 9, 9, 9 };
    pt_null_grain(tiny, sizeof(tiny)); /* no room for a burst: zeros */
    CHECK(tiny[0] == 0 && tiny[3] == 0);
}

int main(void)
{
    test_sink_parse();
    test_kind_for();
    test_carriers();
    test_plan();
    test_schedule();
    test_null_grain();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
