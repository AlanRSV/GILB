# ReChord roadmap and status (Echo Mini, 512 MB and 8 GB, 2026-11-23)

This is the current picture. `STATUS.md` and `HANDOVER.md` are the history of the earlier
approaches (SDK splice, hybrid fw1/BB) and are kept for reference only.

**Both boards.** This line was built on the 512 MB Echo Mini (`MINIV*.IMG` firmware) and runs
on the 8 GB one (`HIFIEC*.IMG`) since 2026-11-23. The differences are documented in
[BOARD_DIFF_512M.md](BOARD_DIFF_512M.md), from a comparison of the stock 512 MB and 8 GB
firmware, and selected at build time (`BOARD=512m | 8g`). Most device testing was done on one
512 MB unit; the 8 GB rows below say so.

How each part was learned is in [Sources](#sources): mainly the stock firmware, with RECHO as
a second reference for some parts.

## Where it stands

"Device" means verified on a real unit (the 512 MB one unless it says 8 GB) and recorded in its
own log (`ECHOLOG.TXT`).
"QEMU" means covered by the milestone suite on the `echo-mini` machine (strict hardware
models with checkers). That suite and the machine live in a separate repository, not
published yet.

| Area | State | Verified |
|---|---|---|
| Boot: ROM -> our reset -> our main loop, 512 MB board port, boot guard, boot trail, watchdog | done | device, QEMU |
| Clocks: 200 MHz PLL, SysTick time base, WFI idle, power states (UI / IDLE / DAC) | done | device, QEMU |
| Storage: eMMC and SD NAND (byte/sector addressed, 8/4/1-bit), microSD, FAT16/32/exFAT, FAT32 writes on the internal volume | done | device, QEMU |
| Audio engine: dual core (core B decodes, DMA to I2S), gapless (bit-exact across track boundaries and sample-rate changes), CS43131 volume | done | device, QEMU |
| Codecs on core B, one image per family loaded from the firmware slot: FLAC, WAV/AIFF/AIFC (int and float), MP3, AAC (LC, HE v1/v2), ALAC, Ogg Vorbis, Opus, WavPack, APE, WMA v1/v2, WMA Pro, WMA Lossless, DSD (DSF/DFF) | done | QEMU (bit-exact against reference decoders), FLAC on the device |
| Damaged files never stop the player (bounds at open, decoder abort, core-B supervisor) | done | QEMU |
| Tags: ID3v2.2-2.4, ID3v1, APEv2, Vorbis comments, RIFF INFO, M4A, Ogg | done | QEMU, host tests |
| Player: play queue, sorted browser, resume, play a whole folder | done | device |
| Library: background index per volume (incremental, A/B commit, resumes after a power cut), Songs / Albums / Artists / Genres / Favourites / Playlists, playlists made and edited on the device (M3U8 on the SD card or Internal) | done | QEMU (round6) |
| EQ on core B: stock's presets, a 10-band Custom, crossfaded changes, bit-exact when off | done | QEMU (round6) |
| Settings file `RECHORD.CFG`, log ring flushed to `/Internal/ECHOLOG.TXT` | done | device |
| Screenshot shortcut (VOL+ and VOL- together, then PLAY: `/Internal/SHOTn.BMP`) | done | QEMU |
| Power: backlight PWM, screen off, auto power-off, battery reading and cut-off, charge state | done | device |
| USB mass storage (internal and SD read/write), hot-plug | done (SD writes: QEMU, sd-rw) | device, QEMU |
| USB DAC: UAC2 high-speed asynchronous, bit-perfect 16/24/32-bit at 44.1-192 kHz, VOL keys, media keys to the host, DAC power modes (battery / power only / charge) | done | device, QEMU |
| USB-C earphones: the player as USB host to a UAC1/UAC2 device | done | QEMU |
| Bluetooth (JieLi module, off by default): scan, connect, reconnect, audio routing, earphone keys, settings from earphone key sequences, USB DAC to Bluetooth earphones | done | device (2026-10-17), QEMU; see [BT_DEVICE_NOTES.md](BT_DEVICE_NOTES.md) |
| Updater: power-cut-safe, both slots, slot repair, ROM USB loader fallback | done | QEMU, maskrom flashing on the device |
| UI: no key bar, a status header (play state, volume, Bluetooth icon, battery), 7-row lists, Now Playing on the full height, options, queue, track info, settings, themes, questions that keep the music going, toasts, a hold line; Noto Sans in five sizes with Devanagari; key gestures in milliseconds (tap, hold, 2x, 3x, holds after taps) | device (2026-10-18): a full screen 13 ms after its key is handled on DMA (19-21 on GPIO); a tap's release takes ~50 ms to confirm |
| Display: display list in 16-row bands, dirty rectangles with column windows; VOP + DMA path by default behind a trial guard (stock's bus timing), GPIO writer as the fallback (display=gpio) | device (DMA trial passed every boot, 2026-10-18), QEMU |
| No stalls: storage writes queued behind the audio, settings saved at pause, core B at full speed on resume | device |
| No stalls on the microSD card: its writes through the same queue (runs of blocks as one command), a stuck card failed and unmounted in 3.5 s instead of holding the loop | QEMU (sd-async) |
| 8 GB board: boot, eMMC on the 8-bit bus (the bus test no longer takes CMD19's missing CRC status as a failure; 1 bit, logged, if a wide bus fails), the firmware area from its slots, playback, ReChord's own updater | device, 8 GB (2026-11-23) |
| USB disk at high speed (it ran at full speed, 1 MB/s at most): an album written at 5.7 MB/s, read at 8.2 MB/s; each session logs its writes (receiving vs storing time, sizes) | device, 8 GB (2026-11-23) |
| Now Playing's Lyrics (tags or .lrc, timed or plain) and Up Next views, album covers made on the PC, boot splash (`RECHORD/SPLASH.RGB`) | device, 8 GB (2026-11-23) |
| ReplayGain on core B (Off / Track / Album, tags of every format with tags, peak-limited, sample-exact at gapless boundaries) | device, 8 GB (2026-11-23); simulator |
| `REVERSE_LISTS=1` builds: VOL- moves up in lists | device, 8 GB (2026-11-23) |
| Clock: the battery-backed PCF8563-type chip on I2C1 (GPIO2.12/2.13, as stock uses it), read at boot, the time in the header (12/24-hour, off), set from Echo Library's `RECHORD/TIME.TXT` | device, 8 GB (2026-11-23); simulator |
| Queue kept across power-off (`RECHORD/QUEUE.BIN`: the queue, its order, the song and the time; saved at power-off, screen off and USB disk; each file checked against its directory entry at the start, gone ones dropped), resumed with Resume at power-on: Last track | device, 8 GB (2026-11-23); simulator |
| Spectrum view (Now Playing, BACK tap: its own overlay, viz): a 256-point FFT of the PCM block being heard, 24 bars, peak caps; the module table now two sectors (41 entries) | device, 8 GB (2026-11-23); simulator |
| Library > Albums as a 2 x 2 cover grid (ui/nav.c PK_GRID: the list's model, drawn in the viz overlay; each visible album's cover56.rgb streamed from its file while the screen is flushed) | device, 8 GB (2026-11-23); simulator |
| The grid's cells: the cover and the year, or the name too (Settings > Display > Album grid, `album_grid=year\|name`) | device, 8 GB (2026-11-24); simulator |
| Boot on the 8 GB unit: first screen 0.48 s, a resumed track heard at 0.6 s. The USB wake (the probe's sequence that lets a later cable be seen, ~0.9 s) runs in steps between passes (usb_probe_start/step/abort): no freeze, the cable still seen; stopped and tried again if playback starts meanwhile. The microSD slot: a 128 GB exFAT card up at 0.71 s, 4-bit (an empty slot reads 'card in' and its init fails, -5: harmless) | device, 8 GB (2026-11-23); simulator |
| Desktop simulator (the firmware's UI, library and player on a PC) | Linux, WSL |

## The recovery path comes first

The way back from any bad image is the boot ROM's maskrom:
1. Unplug USB.
2. Press RST with a paperclip.
3. Hold every key except power.
4. Plug USB in.

`rkdeveloptool` can then write any image. The ROM decides this before any of our code runs.
Nothing in ReChord may change what the ROM sees on that path:
- **PMU_SYS_REG3** is the ROM's boot-mode register. The ROM leaves `0x18DF` there at every
  power-on and, as stock's code implies, reads it at the next reset. ReChord never writes it,
  except for the stock-style USB-loader reboot (`0x18AF`). QEMU fails any other write as a
  VIOLATION.
- No other mechanism into the loader will be added, such as RECHO 0.10's `MASKROM.REQ` file.

## Next

### 1. Display speed
- **Tearing:** a screen change shows for one refresh as part old, part new, split on a slant: the
  writes cross the panel's scan (MADCTL 0x60) and nothing syncs them to it. The bus now runs at
  stock's timing (a shorter slant); a tear-free change needs the panel's TE line (Developer >
  Panel TE (test) asks the device whether it is wired) and writes in the scan's order.
- **Tap latency:** a release is confirmed 30..60 ms after the finger lets go (the voltage's rise,
  0..30 ms, then the driver's 20 ms of idle stability; since 2026-11-23 no second sample after it).
  Stock takes it at the first idle reading, but the 8 GB unit's log (344 releases) shows a held key
  reading idle for a moment once in 20..30 presses: taken at once, that would be a phantom x2. The
  gesture lines now end with the blips by length (<= 10 | <= 20 << 10 | <= 30 ms << 20, hex): if
  none read idle past 10 ms, the driver's 20 ms can be 10.

### 2. Features
- Library follow-ups: Years and Composers views.
- Settings and the resume point in a double-buffered, checksummed record that survives a power
  cut mid-write; the log appended and rotated.
- Hold-to-boot. (The battery-empty shutdown is done: power.c's 3.45 V cut-off, "Battery low".)
- Settings from a phone: not through Bluetooth. The JieLi module's firmware is closed, it is an
  audio source only (never discoverable, no LE), and nothing can update it; dropped (2026-11-23).

### 3. Bluetooth follow-ups
- Whether the module answers status polls on a link that came from `CH06`.

## Sources

**The stock firmware is the main source.** We disassembled and decompiled the official
firmware images (MINIV390 and the other 3.x versions, 512 MB and 8 GB) and traced them in
QEMU. That is how we learned:
- the boot path and the ROM entry
- the board differences between 512 MB and 8 GB
- the panel and the keys
- the storage controllers
- the CS43131 DACs and their power order
- the USB PHY tuning and hot-plug behaviour
- **all of the Bluetooth support**: the module's UART link, its power and audio-routing
  pins, every frame stock sends and parses, and its timing

Where the hardware was unclear, the Rockchip RKNanoD SDK, the chip datasheets and the FCC
filing photos settled it.

**RECHO is the second reference.** RECHO is the closed-source custom firmware for the Echo
Mini, at <https://recho.atzkey.workers.dev>; it was earlier released as RefCFW. Its
developers proved first what this hardware can do: gapless, decoding on core B, 200 MHz.
As part of this work we decompiled RECHO 0.9.2 and 0.10.0 and traced them in QEMU to learn
from them.

Several parts of ReChord copy what RECHO does, re-written in our own C:
- the PLL and clock sequence
- the I2S/DMA programming
- the backlight PWM and power behaviour
- the battery reading
- the volume-key shortcuts
- the settings file format and its defaults

Each such place says "RECHO" in a source comment.

Please support RECHO's developers. RECHO's About screen links <https://ko-fi.com/miauk>.
