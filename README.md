# ReChord

A free, from-source custom firmware for the **SNOWSKY (FiiO) Echo Mini**: its own code on both
of the player's cores, from boot to the screen you see. It runs on both boards: the 512 MB line is
its author's daily firmware on a **512 MB** unit, and this fork runs it on an **8 GB** unit since
2026-11-23.

ReChord was started by Bruno Moraga ([brnomt/ReChord](https://github.com/brnomt/ReChord)); the
firmware that runs today was written by dragarok ([dragarok/ReChord](https://github.com/dragarok/ReChord));
this repository is a fork of dragarok's. Who did what: [Sources and acknowledgements](#sources-and-acknowledgements).

<p>
<img src="docs/images/np_hires.png" width="320" alt="Now Playing: a 24/96 FLAC with its badges, bit rate and progress">
<img src="docs/images/music.png" width="320" alt="A folder list with a Devanagari folder name">
</p>
<p>
<img src="docs/images/bt_home.png" width="320" alt="Home with the Bluetooth icon in the header while AirPods are connected">
<img src="docs/images/np_options.png" width="320" alt="The Options sheet over Now Playing">
</p>
<p>
<img src="docs/images/track_info.png" width="320" alt="Track info: format, length, queue position">
<img src="docs/images/theme_paper.png" width="320" alt="The Paper theme, previewed live">
</p>

*Screenshots are QEMU renders of the same firmware image that runs on the device (2x scale;
the panel is 320×170).*

## What it does

**Playback**
- **Gapless and bit-exact**, across track boundaries and sample-rate changes.
- **Formats:** FLAC, WAV/AIFF (integer and float), MP3, AAC/HE-AAC, ALAC, Ogg Vorbis, Opus,
  WavPack, APE, WMA (v1/v2, Pro, Lossless), DSD (DSF/DFF).
- **Decoding:** core B decodes; core A feeds it and the DACs (two CS43131).
- **Library basics:** tags (ID3v2/ID3v1/APEv2/Vorbis/MP4/RIFF), a play queue, play modes, and
  resume after power-off: with Resume at power-on set to Last track, the same queue (a Library
  list, a playlist, shuffle's order) plays on from the same song and time (`RECHORD/QUEUE.BIN`).
- **Library:** Songs, Albums, Artists, Genres, Years (newest first), Favourites and Playlists, indexed in the background
  (it re-reads only what changed, and refreshes by itself when a card goes in or the PC wrote).
  Playlists are made and edited on the player and kept as M3U8 files on the microSD card
  (`/SD/RECHORD`, or Internal: Settings > Playback > Library data). Albums is a 2 x 2 grid of
  covers (each album folder's `cover56.rgb`, made by Echo Library; a letter tile without one),
  each with its release year, or its name and year (Settings > Display > Album grid).
- **EQ** on core B: stock's six presets (Retro, Bass, Heavy, Pop, Jazz, Unique) and a 10-band
  Custom; Off is bit-exact.
- **ReplayGain** on core B (Settings > Playback > ReplayGain: Off, Track, Album): the
  `REPLAYGAIN_*` tags of FLAC, Ogg, MP3 (ID3v2 TXXX or APEv2), APE, WavPack and M4A, read as each
  track is queued, so the gain changes exactly at the gapless boundary; the tagged peak never
  clips; tracks without tags play as they are. Not while USB-C earphones stream (the reader is
  in the overlay window they hold).

**Interface**
- **Boot splash:** from about 0.18 s after power-on to the first screen at 0.4 s: `RECHORD/SPLASH.RGB`
  if it's on Internal (320 × 170 RGB565 little-endian, 108,800 bytes; Echo Library makes it from any
  picture), else "RECHORD" and the build date.
- **Header:** a status line on every screen with play state, volume, a Bluetooth icon while
  earphones are linked, the time, and the battery.
- **Clock:** the player's battery-backed clock chip, kept running while it is off. Shown in the
  header in 12- or 24-hour time (Settings > Display > Clock). Set from Echo Library (Player
  settings > Player clock): it leaves the PC's time in `RECHORD/TIME.TXT`, which the player takes
  as it leaves USB disk mode.
- **Lists:** seven rows, with cursor memory per level.
- **Now Playing:** shows the cover tile, title, artist and album, format and rate badges, bit
  rate, progress, and the next track. Tap BACK to switch views:
  - **Lyrics:** the line being sung, large, with the one before and after. Read from the song's
    tags (FLAC `LYRICS`, MP3 and WAV `USLT`) or a `.lrc` file of the same name beside it. Timed
    (LRC) lyrics follow the song; plain lyrics move through it at an even pace. Up to 4 KB of
    lyrics per song.
  - **Up Next:** the title, artist and album on the left, the next six songs in the queue on the
    right.
  - **Spectrum:** 24 bars, about 170 Hz to 16 kHz, from the audio being heard (25 frames a
    second), with peak caps.
  - Then the cover again. Not available while USB-C earphones are on (they share the memory the
    views load into).
- **Keys:** each key has one meaning everywhere.
  - VOL+/VOL− move in lists and set the volume on Now Playing.
  - PLAY opens or plays.
    - On Now Playing: tap to pause, 2× for next, 3× for previous, hold for next.
  - On Now Playing (also with the screen off): VOL+ twice then hold seeks forward, VOL− twice
    then hold seeks back.
  - BACK goes up; hold BACK for Home. PLAY held on a menu goes to Now Playing.
    - On Now Playing: tap BACK to switch views (MENU goes back to the list, as before).
  - MENU switches between Now Playing and the library; hold MENU for Options.
  - A `REVERSE_LISTS=1` build swaps the list keys: VOL− moves up, VOL+ down.
- **Text and themes:** Noto Sans in five sizes, with Latin, curly quotes and Devanagari, and
  four themes: Classic, Paper, Night and Contrast.
- **Fast:** keys are sampled every 10 ms on a timer and timed in milliseconds. Each key press
  repaints only what changed. On the device, a key reaches the screen in about 8 ms (median).

**Connections**
- **USB disk:** internal storage and the microSD card, both read/write (FAT16, FAT32, exFAT; the player
  writes to the card too: long names, folders, renames, written behind the music like the internal
  storage: a slow or stuck card never stops the keys).
- **USB DAC:** UAC2, bit-perfect 16/24/32-bit at 44.1–192 kHz, with media keys and a choice of
  DAC power source.
- **USB-C earphones:** the player acts as a USB audio host.
- **Bluetooth earphones** (the JieLi module):
  - scan, connect, and reconnect after a drop;
  - earphone keys work;
  - the USB DAC can stream to Bluetooth.

**Never stuck**
- A power-cut-safe updater keeps two firmware slots.
- A boot guard and a crash record catch faults.
- **Power on:** hold POWER about 1 second; a quick tap (a bump in a pocket) switches it straight
  back off. Not needed with a cable in.
- **Recovery:** hold VOL+ at power-on.
- **USB loader:** hold VOL− at power-on.
- **The boot ROM's maskrom** is always the last resort, and nothing in ReChord touches what it
  reads at reset.

## Status

"Device" means verified on a real unit (the 512 MB one unless it says 8 GB) and recorded in its
own log; "QEMU" means covered by the emulator test suite. The full table is in
[docs/ROADMAP.md](docs/ROADMAP.md).

| Area | Verified |
|---|---|
| Boot, clocks, storage (SD NAND / eMMC / microSD, FAT/exFAT) | device |
| Gapless dual-core audio, FLAC (all formats bit-exact in QEMU) | device, QEMU |
| New UI, fonts, gestures, themes | device (2026-10-18) |
| USB disk, USB DAC, power, battery | device |
| Bluetooth earphones, reconnect, routing | device |
| VOP + DMA display path (display=dma), seek (VOL 2x + hold), fast boot | device (2026-10-18) |
| USB-C earphones | QEMU |
| 8 GB board: boot, eMMC on the 8-bit bus, playback, ReChord's own updater; USB disk at high speed (writes 5.7-5.9 MB/s, reads 8 MB/s) | device, 8 GB (2026-11-23) |
| Lyrics, Up Next and Spectrum views, boot splash, ReplayGain, the clock, the queue kept across power-off | device, 8 GB (2026-11-23) |
| Boot: first screen at 0.48 s, a resumed track playing at 0.6 s; the USB wake in steps (no freeze); the microSD card (exFAT, 4-bit) | device, 8 GB (2026-11-23) |

**Both boards.** The differences between the 512 MB and 8 GB boards come from comparing the stock
firmware of both. They're documented in [docs/BOARD_DIFF_512M.md](docs/BOARD_DIFF_512M.md) and
selected at build time (`BOARD=512m | 8g`). The factory 8 GB unit runs its eMMC on the 8-bit
bus, as stock does; if a wide bus ever fails, the 8 GB build falls back to 1 bit and logs why (see
that document). Keep maskrom recovery at hand.

**Next:**
- a key's way to the screen, measured in parts on the device (key->px: in, loop, wait, render,
  push): a full screen is 13 ms on DMA; most of the ~70 ms after a tap is the release's
  confirmation (~50 ms), not the display
- an on-device self-test mode
- hardware decoding

See [docs/ROADMAP.md](docs/ROADMAP.md).

## Install

> Keep the stock firmware at hand. Flashing modified firmware is at your own risk.

- **Updater:** copy `MINIV900.IMG` (512 MB board) or `HIFIEC90.IMG` (8 GB board) to the player's
  root, eject, and confirm on the player. Stock's updater takes it the first time; after that
  ReChord's own does it (it shows the file's SHA-256; PLAY installs).
- **Maskrom:**
  1. Unplug USB, press RST with a paperclip, hold every key except power, and plug USB in.
  2. Pad the image to 512-byte sectors and write it with `rkdeveloptool wl` to both firmware
     slots, LBA `0` and `0x11000` (both boards, as the boot ROM's loader numbers them).
  3. Read both slots back and compare them.

  Use only `ld`, `td`, `rid`, `rfi`, `rl`, `wl` and `rd`; never `db`. See
  [docs/DEBRICK-LINUX.md](docs/DEBRICK-LINUX.md).

Stock goes back the same way.

## Build

```bash
export PATH=<arm-gnu-toolchain>/bin:$PATH
make cfw-ap -j8                               # core A: build/cfw/rechord_ap.elf
make cfw-img FONT_IMG=<stock MINIV390.IMG>    # + core-B codec images and the fonts module
# -> build/MINIV900.IMG (512 MB board); BOARD=8g -> build/HIFIEC90.IMG
# REVERSE_LISTS=1 (cfw-ap, cfw-img, sim): VOL- moves up in lists, VOL+ down
make host-tests                               # host unit tests
make sim                                      # desktop simulator: build/sim/rechord_sim
```

The **simulator** runs the firmware's own UI, library and player code in a window on a PC (Linux or
WSL), with your music in a disk image and the sound through the PC: see [sim/README.md](sim/README.md).

On a filesystem with coarse timestamps (exFAT), build from clean: `rm -rf build/cfw build/coreb`.

## How it is built

- **Core A** (our `fw1`) runs boot, clocks and power, the UI, storage, USB (mass storage,
  UAC2 DAC, host), the Bluetooth module, and the core-B feed.
  - It's clocked by power state.
  - Storage writes go through a queue that never stalls the main loop or the audio.
- **Core B** runs one decoder image per codec family, loaded from the firmware slot when a
  track needs it.
  - It decodes into a PCM ring.
  - Core A's DMA streams that ring to I2S.
- **The display:**
  - a display list rendered in 16-row bands;
  - dirty rectangles pushed through column windows;
  - the VOP + DMA path by default, behind a trial guard, with stock's bus timing; the GPIO bus
    writer as the fallback and the `display=gpio` escape.
- **The mask ROM** is the fixed boot ROM. It isn't replaced, and nothing writes what it reads at
  reset.

```
firmware/
  startup/   core A start-up, main loop, the app glue (ap_ui.c, ap_bt.c, ap_tick.c, ap_bg.c)
  ui/        the frame, lists, screens, navigation (nav.c, screens.c)
  drivers/   clocks, panel (GPIO / VOP + DMA), text engine and fonts, icons, eMMC/SD, USB
             (MSC, UAC2, host), CS43131, JieLi BT
  services/  config, log, input and gestures, FAT/exFAT, tags, queue, updater, boot guard
  audio/     core A's audio engine, container parsers (MP4, ASF, APE)
  coreb/     core B runtime and decoders
  theme/     theme tokens and the built-in themes
docs/        ROADMAP (start here), BT_DEVICE_NOTES, BOARD_DIFF_512M, hardware and RE notes
```

Every change is proven first in a QEMU machine of the player:
- **Strict models:** the panel, VOP/DMA, eMMC/SD NAND, USB, the JieLi module, the DACs and the
  boot ROM's handoff.
- **Checkers:** any hardware misuse fails the run as a "VIOLATION".
- **Mutants:** deliberately broken builds prove each checker fires.
- **A milestone suite:** boot, audio bit-exactness, every codec, USB, Bluetooth, keys, the UI's
  screenshots, recovery.

That machine and the suite live in a separate repository, not published yet.

## Sources and acknowledgements

- **The authors, in order:**
  - **Bruno Moraga** ([brnomt/ReChord](https://github.com/brnomt/ReChord)) started ReChord (from
    2026-07): the from-source idea, the decompilation and reverse engineering of the stock
    firmware (most of `tools/` and `docs/`), and the first firmware's drivers and start-up code that
    the rest was built on. Ideas from his project's later work are noted in
    [docs/UPSTREAM_IDEAS.md](docs/UPSTREAM_IDEAS.md).
  - **dragarok** ([dragarok/ReChord](https://github.com/dragarok/ReChord)) forked it and wrote the
    512 MB line (from 2026-10-14): all of core B (the decoders, the EQ, the HIFI work), and most of
    core A -- the UI, the library, storage, USB, Bluetooth, power and the board port. Where this
    README says "we", it is mostly dragarok's work, on Bruno's ground.
  - **This fork** ([AlanRSV/ReChord](https://github.com/AlanRSV/ReChord), from 2026-11) brings it to
    the 8 GB board (the storage layout, the 8-bit eMMC bus), and adds ReplayGain, the clock, the
    queue kept across power-off, Now Playing's Lyrics, Up Next and Spectrum views, the album
    cover grid, the boot splash, high-speed USB disk, the reversed list keys and the desktop
    simulator; written with Claude (Anthropic), as the commits' co-author lines say.
- **The stock firmware is the main source.** We disassembled and decompiled the official
  firmware images and traced them in QEMU to learn:
  - the boot path and the board differences;
  - the panel, keys, storage, DACs and USB PHY;
  - **all of the Bluetooth support.**

  The Rockchip RKNanoD SDK, the chip datasheets and the FCC filing photos settled what the code
  didn't.
- **RECHO is the second reference.** [RECHO](https://recho.atzkey.workers.dev) is the
  closed-source custom firmware for the Echo Mini (earlier RefCFW), and it showed first what
  this hardware can do. We decompiled RECHO 0.9.2 and 0.10.0 and traced them in QEMU to learn
  from them. Several parts of ReChord copy what RECHO does, rewritten in our own C:
  - the PLL/clock sequence;
  - I2S/DMA;
  - backlight and power;
  - the battery reading;
  - the shortcuts;
  - the settings format and its defaults.

  Those places say "RECHO" in their source comments.

  **Please support RECHO's developers:** <https://ko-fi.com/miauk>.
- **Libraries:**
  - Rockbox (libdemac, libwma, libwmapro)
  - FFmpeg (WMA Lossless)
  - libmad
  - FAAD2
  - Tremor and libogg
  - libopus and opusfile
  - WavPack
  - Apple ALAC (Apache 2.0)
  - Noto Sans fonts (SIL OFL 1.1)

  Their licenses are in their directories.

## License and disclaimer

GPL-3.0 (see `LICENSE`); bundled libraries keep their own licenses. ReChord is independent: it
is not affiliated with FiiO, SNOWSKY, Rockchip or the RECHO developers. No firmware images
(stock or RECHO) are distributed here.
