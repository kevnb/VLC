# VLC-PS5

## Sound

**Passthrough**: the soundbar or receiver decodes the audio; the console only carries it over HDMI, untouched. One of the three Sound output choices (with Stereo and Surround). Not the console's own *Settings → Sound → Audio Format → Dolby/DTS*, which is **Encode**.

**Encode**: the console decodes everything, mixes it, and re-encodes the mix as Dolby or DTS. Loses Atmos objects. Not something this app does.

**Sink**: whatever is at the end of the HDMI chain that decodes: the soundbar or receiver (through the TV's eARC, or directly). What it can decode is what its EDID lists.

**Bitstream**: compressed audio on HDMI, packed as IEC 61937 bursts inside 16-bit PCM frames. A bitstream has one **kind**: Dolby Digital, Dolby Digital Plus, DTS, DTS-HD or Dolby TrueHD.

**Bitstream mode**: the console's HDMI output set up for one kind of bitstream. While it is on, all of the app's PCM is muted. **Normal output** is the opposite: PCM.

**Sticky**: the bitstream mode stays on after a file ends, so the next file of the same kind needs no HDMI change. It goes back to normal output only when PCM has to play (a decoded file, the speaker test) or the app quits.

**Carrier**: the console audio port a kind of bitstream travels on, with its frame size, grain and rate.

**Passthrough delay**: how long the sink takes to decode, which the app makes up for by sending the bitstream that much earlier. Separate from a file's own **Audio delay**.
