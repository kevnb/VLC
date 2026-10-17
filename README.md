<p align="center">
  <img src="assets/vlc-logo.png" alt="VLC" width="140">
</p>

<h1 align="center">VLC</h1>

<p align="center">
  <b>VLC media player, running natively on PlayStation 5.</b><br>
  A port of <a href="https://www.videolan.org/vlc/">VLC</a> 3.0 as a native PS5 app, with a controller-first interface.
</p>

<p align="center">
  <img alt="Version" src="https://img.shields.io/badge/version-0.4-f37a2b">
  <img alt="Platform" src="https://img.shields.io/badge/platform-PS5-1f1f1f">
  <img alt="VLC" src="https://img.shields.io/badge/libvlc-3.0.24-ff8800">
  <img alt="Renderer" src="https://img.shields.io/badge/renderer-Vulkan%20(RADV)-c0561b">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-blue">
</p>

---

## Features

- Plays nearly anything VLC plays: H.264, HEVC (8 and 10-bit), AV1, VP9, VC-1, MPEG-2 and more, up to 4K, with every common audio format, including TrueHD, DTS-HD and Opus.
- **Hardware decoding** on the PS5's video chip for H.264 and HEVC up to 4K, 10-bit HDR included.
- **HDR**: HDR10 and HLG go to an HDR TV as HDR, and are tone mapped with natural colours on an SDR one. Dolby Vision files play as their HDR10 base.
- **Library** with Home, Videos, Music, Playlists and Browse: thumbnails, Continue watching, Favourites and search.
- **Playlists**: make your own, add any video or song from anywhere with Options, reorder them; covers made from their videos; `.m3u` files on your drives play too.
- **Subtitles** in every script, styled ASS/SSA, size and colour, delay, plus **OpenSubtitles** downloads.
- **5.1 / 7.1 surround**, equalizer, night mode and audio delay.
- **Passthrough**: Dolby Digital, Dolby Digital Plus, TrueHD (Atmos included), DTS and DTS-HD go to your soundbar or receiver untouched, for it to decode. See [Passthrough audio](#passthrough-audio).
- **Network**: SMB shares, DLNA servers, network links (HTTP, HLS, DASH…), free TV channels and internet radio.
- **Phone & PC page**: open `http://<ps5-ip>:8080` in any browser to send files to the PS5, download them back to your phone or PC, or add your OpenSubtitles key.
- Music mode with covers and a visualizer, a photo viewer, DVD / Blu-ray folders, and ZIP / RAR / 7z archives.
- Two looks (**Classic** and **Modern**) and **16 languages**.

## Install

You need a PS5 with **kstuff** running, plus **[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus)**.

Download the [latest release](../../releases/latest), then:

1. Copy the **`PPSA85300`** folder to `/data/homebrew/`.
2. Start **VLC** from the home screen.
3. Add your media: a USB drive, `/data/vlc/`, or send files from your pc/phone (Options › Send files from a phone).

Optional full access. With etaHEN, OnionHEN or the Helper, VLC can lift its sandbox. This gives it all of /data and USB drives plugged in while it runs. download [helper.elf](https://github.com/ZiZc3/VLC-PS5/raw/refs/heads/main/patches/Helper.elf)

## Controls

**Library**

| Button | Action |
|---|---|
| D-Pad | Move |
| ✕ | Play / open |
| ○ | Back |
| △ | Favourite |
| □ | Details / delete |
| L1 / R1 | Switch tabs |
| L2 / R2 | Jump A–Z |
| R3 | Search |
| Options | Settings, search, network link, phone… |

**Player**

| Button | Action |
|---|---|
| ✕ | Play / pause |
| ← / → | Back / forward 10 s |
| L2 / R2 | Back / forward 30 s |
| L1 / R1 | Previous / next chapter (or file) |
| △ | Tracks: audio, subtitles, playback, picture |
| □ | Next frame (while paused) |
| L3 | Screenshot |
| Touchpad | Show the controls · swipe to seek or change the volume |
| ○ | Back |

## Passthrough audio

Normally the PS5 decodes a film's sound and sends it to your soundbar or receiver as PCM. That works, but it loses Atmos: the height and object information in Dolby Digital Plus (Atmos) and TrueHD (Atmos) doesn't survive the decode. With **passthrough**, VLC sends Dolby and DTS untouched over HDMI, and the soundbar or receiver decodes them, Atmos included.

### Turning it on

Settings › Sound › **Sound output** › **Passthrough (soundbar decodes)**, or change Sound output in the player's △ menu. It takes effect from the next file you open.

| Format | Goes out as |
|---|---|
| Dolby Digital (AC-3) | Dolby Digital |
| Dolby Digital Plus (E-AC-3), Atmos included | Dolby Digital Plus |
| Dolby TrueHD, Atmos included | Dolby TrueHD (MAT) |
| DTS | DTS |
| DTS-HD (HRA, MA) | DTS-HD, or its DTS core for a sink that only lists DTS |
| Anything else (AAC, FLAC, Opus, MP3, PCM…) | Decoded, as with Surround |

A format only goes out as passthrough if your soundbar or receiver says it can decode it. VLC asks the console for the sink's EDID, the list of formats an HDMI device advertises, each time a file starts. Anything it doesn't list is decoded exactly as with **Surround**, so a file never plays silent because of passthrough. With the PS5 plugged into a TV and the soundbar on the TV's eARC port, the TV passes the soundbar's list on. A dock or switch in between may cut that list down to its own.

### While the soundbar decodes

- **Volume** is your soundbar's or receiver's, through its remote or the TV's (HDMI-CEC). VLC's volume, the equalizer and night mode can't reach a bitstream, so the △ menu shows *Decoded by your soundbar* in their place.
- **Audio delay** still works per file.
- **Passthrough delay** (Settings › Sound, 0–500 ms) makes up for the time the soundbar takes to decode, which PCM doesn't have. VLC sends the sound that much earlier. Start at 0 and raise it if voices come after lips.
- **The speaker test** plays PCM. It works whenever no passthrough file is playing.

### How it works

- **The format is negotiated, not decoded.** VLC's `spdif` decoder hands the compressed frames to the app's audio output, `ps5/modules/ps5aout.c`. That module accepts them when passthrough is on and the sink lists the format. VLC's own `tospdif` converter then packs them as **IEC 61937** bursts, the standard way compressed audio travels inside HDMI's PCM channels.
- **The bursts go to the console's bitstream ports.** `src/platform_ps5.c` opens the matching port and switches the HDMI output to that format, in the order Sony's own media apps use:

  | Format | Console API and mode | Port |
  |---|---|---|
  | Dolby Digital | `sceAudioOutExOpen` mode 0 | 16-bit stereo, 48 kHz |
  | DTS | Ex mode 2 | 16-bit stereo, 48 kHz |
  | Dolby Digital Plus | Ex mode 3 | 16-bit stereo, 192 kHz |
  | DTS-HD | Ex mode 4 | 16-bit stereo, 192 kHz |
  | Dolby TrueHD | `sceAudioOutSysOpen` mode 5 | 16-bit 8-channel, 192 kHz |

- **Lip sync works as for PCM.** A writer thread sends each burst when VLC's clock says it's due: null data before it, dropped whole if it's late.
- **The HDMI mode is sticky.** Receivers can get confused by format changes in quick succession, so VLC keeps the bitstream mode on from one file to the next of the same format. It goes back to normal PCM output only when something has to play as PCM: a decoded file, the speaker test, or quitting VLC. Each change gives the receiver a short settle time, so a file that changes the format can take a second or so longer to start.
- **It runs inside the normal sandbox.** It needs no jailbreak features beyond what VLC already uses.

The decisions (EDID parsing, which format goes out how, ports, when the mode changes) are in `src/passthrough.c`. They're tested on a PC with `cc -std=c11 -Isrc tests/passthrough_test.c src/passthrough.c -o /tmp/pt && /tmp/pt`.

### If something's wrong

- **Check the log first.** `/data/homebrew/PPSA85300/vlc-ps5.log` has a line `passthrough: sink "…" lists …`, which shows the formats the console sees. The CEA-861 coding types are 2 AC-3, 7 DTS, 10 E-AC-3, 11 DTS-HD and 12 TrueHD/MAT. If a format is missing there, the TV, a dock or the HDMI port isn't passing it on.
- **The soundbar shows the wrong format or stays silent after many format changes:** restart the soundbar. This is a known receiver-side behaviour from the research below, and the sticky mode makes it rarer.
- **Sound is early or late with passthrough only:** adjust Passthrough delay.

The console side follows [PS5-Audio-Passthrough-Research](https://github.com/sainsaji/PS5-Audio-Passthrough-Research), which found and verified these calls on firmware 12.70.

## Folders on the PS5

| Path | What |
|---|---|
| `/data/homebrew/PPSA85300/media/` | The app's own media folder |
| `/data/vlc/` | Your media on the console (needs full access) |
| `/mnt/usb0` … `/mnt/usb7`, `/mnt/ext0` | USB and extended storage |
| `/data/homebrew/PPSA85300/vlc-ps5.log` | Log of the last session (attach it when reporting a problem) |

## All VLC Features

✅ In the app.

### 🎞️ Video

| Feature | Status |
|---|---|
| H.264, HEVC 8/10-bit, AV1, VP9, VC-1, MPEG-2, MPEG-4 and every format FFmpeg knows | ✅ |
| MKV, MP4, AVI, MOV, TS, WebM, FLV, OGG… | ✅ |
| Up to 4K at 60 Hz output | ✅ |
| Auto fast decoding when a file can't keep up | ✅ |
| Fast keyframe seeking | ✅ |
| Aspect ratios (fit, fill, stretch, 16:9, 4:3, 21:9) and zoom | ✅ |
| Deinterlace (auto / yadif) | ✅ |
| Picture controls: brightness, contrast, saturation, gamma, hue | ✅ |
| Rotate, flip, sharpen | ✅ |
| 360° videos (look around with the right stick) | ✅ |
| Frame-by-frame step | ✅ |
| Screenshots to PNG | ✅ |
| Hardware decoding (H.264 / HEVC) | ✅ |
| HDR → SDR tone mapping | ✅ |
| HDR10 output to the TV | ✅ |
| Hardware decoding (VP9 up to 4K) | Not yet |
| Upscaling up to 4K (FSR 1, Lanczos, Anime4K etc) | Not yet |

### 🔊 Audio

| Feature | Status |
|---|---|
| AAC, AC-3, E-AC-3, DTS, DTS-HD, TrueHD, FLAC, ALAC, Opus, Vorbis, MP3 | ✅ |
| 5.1 / 7.1 surround output | ✅ |
| Speaker test | ✅ |
| Stereo downmix for headphones | ✅ |
| Audio delay (lip sync) | ✅ |
| 10-band equalizer with presets | ✅ |
| Night mode (compressor) | ✅ |
| Volume boost up to 200 % | ✅ |
| Preferred audio language | ✅ |
| [Passthrough](#passthrough-audio) to a soundbar or receiver: Dolby Digital, Dolby Digital Plus (Atmos), TrueHD (Atmos), DTS, DTS-HD | Testing |

### 💬 Subtitles

| Feature | Status |
|---|---|
| SRT, ASS/SSA, embedded MKV/MP4, PGS, DVB, DVD, closed captions | ✅ |
| Every script: Arabic, Hebrew, Cyrillic, CJK, Thai, Devanagari… | ✅ |
| Styled ASS/SSA (libass: fonts, positions, karaoke) | ✅ |
| Auto-load `movie.srt` next to the video | ✅ |
| Add a subtitle file while playing | ✅ |
| Subtitle delay | ✅ |
| Size, colour, background box | ✅ |
| Preferred subtitle language | ✅ |
| OpenSubtitles download (your own free API key) | ✅ |

### ▶️ Playback

| Feature | Status |
|---|---|
| Resume and Continue watching | ✅ |
| Seek preview while scrubbing | ✅ |
| Speed 0.25× – 4× | ✅ |
| Playlists | ✅ |
| Chapters (list, skip, marks on the seek bar) | ✅ |
| Bookmarks inside a file | ✅ |
| A–B repeat | ✅ |
| Loop, repeat list, shuffle | ✅ |
| Playlists (.m3u, .m3u8, .pls) | ✅ |
| Up next with countdown, Watch again | ✅ |
| Sleep timer | ✅ |
| Touchpad swipes (seek, volume) | ✅ |

### 📚 Library & interface

| Feature | Status |
|---|---|
| Home, Videos, Music, Browse tabs with thumbnails | ✅ |
| Classic and Modern looks | ✅ |
| Colour themes (Modern) + custom theme file | ✅ |
| 16 languages (follows the console's language) | ✅ |
| First-run setup | ✅ |
| Favourites | ✅ |
| Search with on-screen keyboard | ✅ |
| A–Z jump in long lists | ✅ |
| File details (codec, resolution, bitrate…) | ✅ |
| Delete files and folders | ✅ |
| USB drives detected while running | ✅ |
| Music mode: covers, artist / album, plays behind the menus | ✅ |
| Audio visualizer | ✅ |
| Photo viewer with slideshow | ✅ |
| Text / .nfo viewer | ✅ |

### 🌐 Network

| Feature | Status |
|---|---|
| PC/Phone page: send files over Wi-Fi | ✅ |
| SMB shares (Windows, NAS) with login | ✅ |
| DLNA / UPnP media servers | ✅ |
| Network links: HTTP, HTTPS, HLS, DASH, UDP, RTP, FTP | ✅ |
| Free TV channels (iptv-org) | ✅ |
| Internet radio (radio-browser) | ✅ |

### 💿 Discs & files

| Feature | Status |
|---|---|
| DVD folders and ISOs with menus | ✅ |
| Blu-ray folders and ISOs (unprotected) | ✅ |
| Blu-ray Java (BD-J) menus | ❌ needs Java, which the PS5 hasn't got: those discs play without menus |
| ZIP, RAR and 7z archives open like folders | ✅ |

## Building

On Linux (WSL2 Ubuntu works), with [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) built in `~/ps5/PS5_Vulkan`:

```sh
mkdir -p ~/ps5/vlc-ps5/src && cd ~/ps5/vlc-ps5/src
curl -LO https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz
tar xf vlc-3.0.24.tar.xz && cd -               # back to this repository

TARGET=ps5 bash ps5/deps/build-deps.sh     # the libraries (FFmpeg, GnuTLS, libass…)
TARGET=ps5 bash ps5/configure-vlc.sh       # libvlc 3.0.24 with every plugin linked in
bash ps5/build-title.sh                    # dist/PPSA85300/ with a signed eboot.bin
```

The libraries are downloaded by `build-deps.sh`; the changes to VLC and FFmpeg are in [`patches/`](patches). `bash host/build-app.sh` builds a Linux version of the app for testing.

Or let GitHub build it: on a fork, **Actions › Build PS5 title › Run workflow** runs these same steps and gives you the `PPSA85300` folder as a download (`.github/workflows/build-ps5.yml`).

## Credits

- **[VLC](https://www.videolan.org/)** by VideoLAN and its contributors, and **[FFmpeg](https://ffmpeg.org/)**.
- **[Mihawk-99](https://github.com/mihawk-99)**: [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan), RADV on the PS5.
- **[sainsaji](https://github.com/sainsaji)**: [PS5-Audio-Passthrough-Research](https://github.com/sainsaji/PS5-Audio-Passthrough-Research), HDMI bitstream output from homebrew.
- **[ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk)**, **[kstuff](https://github.com/EchoStretch/kstuff)**, **[etaHEN](https://github.com/LightningMods/etaHEN)**, **[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus)** and the PS5 scene.
- **[Dear ImGui](https://github.com/ocornut/imgui)**, [volk](https://github.com/zeux/volk), [stb](https://github.com/nothings/stb), GnuTLS, libsmb2, libupnp, libxml2, libass, FreeType, FriBidi, HarfBuzz, libdvdnav, libbluray and libarchive.
- Fonts: **[Inter](https://rsms.me/inter/)**, **Selawik**, **DejaVu Sans Mono** and **Noto**.
- [iptv-org](https://github.com/iptv-org/iptv), [radio-browser.info](https://www.radio-browser.info/) and [OpenSubtitles](https://www.opensubtitles.com/).

## License

VLC for PS5 is licensed under the **GNU General Public License v3.0** (see [`LICENSE`](LICENSE)).
VLC itself is GPL-2.0-or-later / LGPL-2.1-or-later. The bundled libraries and fonts keep their own licenses.
