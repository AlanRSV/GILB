# Programs: running code from a file (2026-11-26)

Settings › System › Programs lists the `.APP` files in `GILB/APPS` on the internal storage and
runs the one picked. A program is our own format (`sdk/include/gilb_app.h`), built with the SDK
(`sdk/README.md`). This was the first step towards games such as the Doom port, which RECHO already
runs from its internal storage. The steps after it are Doom, sound in the API, and the USB control
channel.

## Where it lives

| What | Where |
|---|---|
| The loader, the list, the API table | `firmware/services/programs.c`, the whole file is the `prg` overlay |
| The overlay | the library job's phase window (OVL_LPH, 12 KiB at 0x0304C000), `firmware_ap.ld` `.ovl_prg` |
| Its gate | `startup/ap_ovl.c` `ap_programs()`, the menu row in `ui/screens.c` `g_system` |
| Taking the other windows | `services/ovl.c` `ovl_take()` |
| The format and the API | `sdk/include/gilb_app.h` (shared by the firmware and programs) |

Resident cost: the gate, the menu row and `ovl_take`. Resident RAM: 12 bytes, because the list
lives on the stack. An overlay's `.bss` is resident, and resident RAM is nearly full.

## The memory a program gets

`0x03030000..0x0304C000`, 112 KiB, contiguous:

| Range | Normally | While a program runs |
|---|---|---|
| 0x03030000..0x03040000 | the audio input ring | the program (music is stopped first: `player_stop`) |
| 0x03040000..0x03043800 | the modes' window | the program (its overlay leaves: stop hook, hooks cleared) |
| 0x03043800..0x03045800 | the file system's window (fsw) | the program (so: no exFAT writes, no new long names) |
| 0x03045800..0x0304C000 | the library's window | the program (the library's `.bss` stays resident) |
| 0x0304C000..0x0304F000 | the phase window | **the loader itself and the API table**: untouched |

`ovl_take(1)` evicts the first three windows the normal way (stop hooks, the window checked
unchanged, hooks into it cleared). It also marks them empty and refuses any load into them
("busy") until `ovl_take(0)`. Afterwards each overlay loads again on its next `ovl_get()`. The
library's callers only use `ovl_if`, so the loader reloads the library itself. Reloading puts the
same bytes at the same addresses, so pointers that the library's or the job's resident state keep
into their code stay valid. That is why a library job in progress survives a program: `layb`
already evicts its phase between steps.

Not used, and kept for later (Doom will need more than 112 KiB):

- the fonts window (97.5 KiB): GILB would reload the fonts afterwards
- the PCM ring (64 KiB): the audio DMA must be stopped first, or it plays the program as noise
- core B's RAM (0x01000000..0x01060000, 384 KiB): core B would have to be held

## Why it returns instead of restarting

A soft reset can do nothing on this chip (`services/bootguard.c`: the 8 GB unit's verdict #24), so
"run, then restart into GILB" isn't reliable. Nothing the program can touch belongs to GILB's
running state: resident code, `.data`, `.bss`, the stack's base and the loader's window all stay. So
when the entry returns, the loader:

1. calls `ovl_take(0)`
2. reloads the library's overlay
3. restores the user's brightness
4. drains the key queue
5. redraws its list

## Checks before a program runs

Before anything runs, the loader checks:

- the header: its magic and size, the API version the program needs, and that it was linked at
  `GILB_APP_BASE`
- the length: the header's must match the file's, and the file and its `.bss` must fit
- the entry: a Thumb address inside the program
- the CRC-32 of every byte read: the updater's, poly 0x04C10DB7, with the crc field as 0

A refusal says why, on the screen and in the log (`programs: NAME.APP refused: ...`). A run logs
`programs: NAME.APP start (N B, M B of memory)` and `... ended (rc)`.

## The API (version 1)

Function pointers in one table, `gilb_api_t`. New ones are only ever added at the end, and
`version` goes up. A program asks for a minimum version in its header.

- **Screen:** `blit` goes straight to the panel (`panel_io_blit`, 16-row pieces, the display's own
  band size). It doesn't use the display list.
- **Buttons:** `key` (GILB's gestures, `input_next`).
- **The rest:** `poll` (`input_scan` + `ap_pump`, and POWER held 2 s means "leave"), `ms`,
  `sleep_ms`, the files (`fs_*`), `log`, `glyph` and `backlight`.

While a program runs, the main loop doesn't. So there's no screen-off timer, no auto power-off and
no low-battery shutdown. `ap_pump` still runs the storage queue, the log, Bluetooth and the
display trial.

## Testing

- `make -C sdk APP=hello NAME=Hello`, then copy `sdk/build/HELLO.APP` to `GILB/APPS`.
- Hello tests drawing, keys, files (its run count) and leaving.
- Check that GILB is fine afterwards:
  - music plays
  - Library opens
  - lyrics or the spectrum load (the modes' window)
  - a new long-named playlist saves (fsw)

## API version 2 (2026-11-26): more memory

`more_mem` stops the music, parks core B and holds it in reset (`audio_b_free`, which follows the
USB DAC's path), then lends its RAM in two ranges around the shared block at 0x01020000:

- 0x01000000, 128 KiB
- 0x01021000, 252 KiB

The next play loads core B again (device-checked). `sdk/examples/memtest` measured it: data is
1.6x slower to read there and code runs 4.8x slower there (docs/DOOM_PLAN.md). That makes it
right for big data and for code that runs rarely. `sdk/app.ld` gathers `.movable` sections between
`__movable_start` and `__movable_end`, for a program that copies code there itself.

## API version 3 (2026-11-26): core A's fonts window and PCM ring

`more_mem_a` lends one more range, 0x03077900..0x030A0000 (161.75 KiB): the fonts window, the
audio DMA's list (`AS_LLI`) and the PCM ring, which lie next to each other. It stops the music
and the audio DMA (`audio_pcm_free`: `audio_b_free`, then the DMA and the USB DAC's source off).
While the program runs GILB has only its 8x8 font (`glyph`); when the entry returns, `run()`
loads the fonts module again (`text_fonts_load`). The next play sets up the DMA list and the ring.
Doom (`ports/doom/`) uses it.

Also fixed then: `a_poll` kept POWER's press time as `now | 1`, so on an even millisecond the
unsigned `now - t0` was huge and a tap counted as "held 2 s". It compares signed now.

