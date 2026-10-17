# HDMI passthrough: agreed decisions (grill-with-docs, 2026-10-17)

The recommended answer was taken for every question.

1. Goal: Atmos on the soundbar, so Dolby Digital Plus (JOC) and TrueHD first. Dolby Digital, DTS and DTS-HD go out as passthrough too when the sink lists them. AAC and everything else is decoded.
2. Signal chain: read the sink's EDID (sceAudioOutSysGetHdmiMonitorInfo) and pass through only the formats it lists. Everything else falls back to decoding.
3. Build: the code lands as a branch/patch for the user to build with PS5_Vulkan on their own Linux/WSL2 machine.
4. Architecture: a new static audio output module `ps5aout` alongside ps5vdec, replacing amem. For PCM it behaves exactly like amem, using the same callbacks (the queue, audio thread, EQ and timing don't change). For Dolby/DTS it negotiates S/PDIF so VLC's tospdif does the IEC 61937 packing. It uses the research repo's call order: open the port before configuring, target 1, the Ex API for AC-3/E-AC-3/DTS/DTS-HD (modes 0/3/2/4), and the Sys API mode 5 for TrueHD.
5. Switching policy: sticky. Keep the bitstream mode across files of the same kind, and switch only when the kind changes. Reset to normal output only when PCM must play, for the speaker test, and on exit.
6. Features that can't apply on a bitstream: volume, boost, EQ and night mode are hidden or marked "decoded by your soundbar". Audio delay keeps working. The speaker test always plays as PCM.
7. Soundbar decode delay: a separate "Passthrough delay" setting, applied automatically while bitstream is on, added to the per-file audio delay. Starts at 0 ms.
8. Upstream-ready: off by default, English strings with the other languages falling back to English, no hardcoded format choices, and the repo's comment style.
