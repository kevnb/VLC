# 1. Sound goes through our own audio output, ps5aout, not VLC's amem

## Status

Accepted, 2026-10-17.

## Context

The app played all sound through libvlc's audio callbacks, which select VLC's `amem` output. Passthrough needs VLC to hand over compressed Dolby/DTS frames, but `amem` only negotiates PCM (it answers every format with S16), so VLC always decoded.

The alternatives were:

- rewriting the whole sound path as a VLC audio output;
- catching the compressed packets before VLC's audio output (a demux or decoder hook) and packing them with our own IEC 61937 code, as PS5-Audio-Passthrough-Research's sample app does;
- a small audio output that does what `amem` does for PCM and also accepts compressed formats.

## Decision

`ps5/modules/ps5aout.c` is the app's audio output on the console. For PCM it calls the same `amem-*` callbacks libvlc sets, so `player.cc`'s queue, mixing, night mode and timing are unchanged. For Dolby/DTS, when the sound output is Passthrough and the sink's EDID lists the format, it negotiates S/PDIF at the console port's rate and layout. VLC then puts its own `tospdif` converter in front, which packs AC-3, E-AC-3, DTS, DTS-HD (Type IV) and TrueHD (MAT), and `ps5aout` sends the bursts on their play times.

## Consequences

- No IEC 61937 packing code of our own: VLC's is used, with its MAT framing for TrueHD.
- Anything `ps5aout` refuses (codec, sink, console error) makes VLC decode the file, so failure falls back to PCM.
- `player.cc` overrides the `aout` variable after setting the callbacks (console build only). A libvlc upgrade that changes how the callbacks select `amem` has to keep that order working.
- The host build still uses `amem`, so it can't exercise passthrough. `src/passthrough.c` holds the decisions and is tested on a PC instead (`tests/passthrough_test.c`).
