<!-- Draft notes for the first GILB release. Paste everything below the line into GitHub's release
     notes (the download site shows them under "What's new"). Before publishing: rebuild with
     `sh release/build.sh` on the final commit and put its commit and SHA-256 values below. -->

---

**GILB** is a free, open-source custom firmware for the **SNOWSKY (FiiO) Echo Mini**, **8 GB board**.
It's a fork of [ReChord](https://github.com/dragarok/ReChord), which learned from
[RECHO](https://recho.atzkey.workers.dev), the first custom firmware for this player, by **miauk**
(please support them: <https://ko-fi.com/miauk>).

This is the first public release. Built from commit `COMMIT` of this repository, which is its source code (GPL-3.0).

## What's in it

- Gapless, bit-exact playback: FLAC, WAV/AIFF, MP3, AAC, ALAC, Ogg Vorbis, Opus, WavPack, APE, WMA
  (v1/v2, Pro, Lossless), DSD.
- A library with Songs, Albums (a cover grid), Artists, Genres, Years, Favourites and Playlists;
  ReplayGain; a 10-band EQ; lyrics, Up Next and a spectrum on Now Playing.
- Two looks, Modern and Retro, with themes and layouts you can make in Echo Library.
- USB disk, USB DAC (up to 192 kHz), USB-C earphones and Bluetooth earphones.
- A self-test, a power-cut-safe updater and recovery modes.

The full list is in the [README](https://github.com/AlanRSV/GILB#what-it-does).

## Files

| File | What it is | SHA-256 |
|---|---|---|
| `HIFIEC90.IMG` | the firmware, **8 GB board only** | `SHA_IMG` |
| `CJK.BIN` | optional: Japanese, Chinese and Korean text | `SHA_CJK` |
| `OFL.txt` | the font license for `CJK.BIN` | |

## Install

1. Check that your player is the **8 GB** model.
2. Connect it to your computer and choose **USB disk**.
3. Copy `HIFIEC90.IMG` to the root of the player's internal storage.
4. Eject the drive. The player asks to update: confirm. The first time, the stock updater installs it.
   After that, GILB's own updater shows the file's SHA-256 (compare it with the table above) and
   PLAY installs.
5. Optional: copy `CJK.BIN` to the `GILB` folder on the internal storage (`GILB/CJK.BIN`) for
   Japanese, Chinese and Korean titles.

Going back to stock works the same way, with SNOWSKY's own update file.

## Known limits

- **512 MB board:** not supported in this release (never tested).
- **Bluetooth:** SBC only. The Bluetooth module takes the player's analog output and has its own
  firmware, so better codecs aren't possible.
- AAC, Vorbis and WMA are decoded in software.

## Risk

Installing custom firmware is at your own risk. GILB has been tested on two 8 GB players, including a
first install through the stock 4.0.0 updater, but it isn't made or supported by SNOWSKY or FiiO.
If something goes wrong, hold VOL+ at power-on for recovery; the boot ROM's maskrom mode is always the
last resort (`docs/FLASHING.md`, `docs/DEBRICK-LINUX.md`).

## Credits and licenses

- **RECHO** by miauk: the first Echo Mini custom firmware, which ReChord studied.
- **ReChord**: started by Bruno Moraga, the firmware GILB builds on written by dragarok.
- Decoders: libmad, FAAD2, Rockbox (libdemac, libwma, libwmapro), FFmpeg (WMA Lossless), Tremor and
  libogg, libopus and opusfile, WavPack, Apple ALAC, dr_flac.
- Fonts: Noto Sans (SIL OFL 1.1); Fusion Pixel by TakWolf for `CJK.BIN` (SIL OFL 1.1, `OFL.txt`).

GILB is GPL-3.0. Each bundled library keeps its own license:
[THIRD_PARTY.md](https://github.com/AlanRSV/GILB/blob/main/THIRD_PARTY.md). No stock firmware and no
part of RECHO is included.
