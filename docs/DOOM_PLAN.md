# Doom on GILB: the plan (2026-11-26)

Doom would run as a GILB program (docs/PROGRAMS.md), ported from Nicola Wrachien's
[MG24 Doom BLE](https://github.com/next-hack/MG24_Doom_BLE). That port is itself based on doomhack's
GBA Doom, and it is the one a video showed running on RECHO.

## The source

- **License:** the Doom code is GPLv2 or later, and GILB is GPL-3.0, so that works. Marco Paland's printf
  is MIT and Daniel Hepper's font8x8 is public domain. Silicon Labs' GSDK isn't in the repo (MSLA),
  and we don't need it: it's the chip's drivers.
- **Clones:** clone it inside WSL (`~/ext/mg24doom`). Its `.pdm` file names are too long for a Windows
  checkout.
- **Layout:**
  - `DoomMG24BLE/Doom/{source,include}`: 58 files, the game
  - `DoomMG24BLE/src`: the board's drivers (display over SPI, keys, audio PWM, BLE, external flash)
  - `MCUDoomWadUtil`: a PC tool that converts `DOOM1.WAD` into the port's own WAD layout

## How it reads the game data

Through `src/extMemory.h`, a small API over an external SPI flash:

- `extMemGetByteFromAddress`, `extMemFlashGetShortFromAddress`
- `extMemSetCurrentAddress` and `extMemGetDataFromCurrentAddress`
- `extMemStartAsynchDataRead` / `extMemWaitAsynchDataRead`
- `extMemWrite` / `extMemErase` / `extMemProgram`, for save games and the texture cache

An address there is an offset into the converted WAD. On GILB that becomes a seek and a read in a file
on the internal storage, with a small read cache, because the eMMC is slower per request than SPI flash.
13 game files use it.

The port also caches composite textures in the MG24's internal flash (`DISABLE_CACHING_TEXTURE_TO_FLASH`
in `src/main.h`). On GILB that cache has to live in RAM.

## Its size (compiled for the player's Cortex-M3, -Os, 2026-11-26)

53 of the 58 game files compile with header stubs. The other 5 need more hardware stubs:
`d_client` (BLE), `d_main`, `i_main`, `r_data` (the flash cache) and `w_wad`.

| | Bytes |
|---|---|
| code + constants (53 files) | 312,903 (with the other 5: about 340 KB) |
| of which `tables.o`, the math tables | 82,124 |
| of which `opl2.o`, the music | 57,628 (left out of a silent first build) |
| `.bss` | 78,760 (`z_zone.o`'s heap: 62,301) |

On the MG24, it all fits 256 KB of RAM and about 0.5 MB of internal flash.

## Where it could fit on the player

| Memory | Size | Note |
|---|---|---|
| program region 0x03030000..0x0304C000 | 112 KiB | what programs get today |
| fonts window 0x03077900..0x0308FF00 | 97.5 KiB | GILB reloads the fonts afterwards |
| PCM ring 0x03090000..0x030A0000 | 64 KiB | only with the audio DMA stopped |
| core B's RAM 0x01000000..0x01060000 | 384 KiB | only with core B held; GILB restarts it afterwards |

That's about 650 KiB in all. The plan:

- the code (about 280 KB without the music) goes in core B's RAM
- data, `.bss` and the zone heap go in the program region and the fonts window / PCM ring

0x01000000 is in the Cortex-M3's code region. Whether core A can run code from core B's RAM, and at
what speed, has to be **tested first** with a small program.

## The memory test (sdk/examples/memtest, device 2026-11-26, 8 GB)

The firmware has `api->more_mem` (API version 2), which holds core B (`audio_b_free`) and lends its
RAM. The results:

| Test | Result |
|---|---|
| write/read 380 KiB (0x01000000 +128 KiB, 0x01021000 +252 KiB) | 0 bad words |
| a loop, its code in core A's RAM (2,000,000 passes) | 313 ms |
| the same loop, copied to core B's RAM | 1501 ms, the same result: **it runs, 4.8x slower** |
| the code in A, the data it reads in B's RAM | 500 ms: **reading data there is 1.6x slower** |

Music played normally afterwards (device, 2026-11-26): core B loads again at the next play.

So core B's RAM can hold code, but only code that runs rarely, such as:

- the menus
- level loading
- the finale and intermission
- the status bar's logic

The renderer, the fixed-point math and the tables it reads every frame belong in core A's RAM:

- the program region: 112 KiB
- the fonts window: 97.5 KiB
- the PCM ring: 64 KiB, while silent

The linker places it all by section (two code regions, like GILB's own overlays). Still to try:
whether B's RAM gets faster with core B's clock raised (`clock_set_core_b_div`). Its bus may run
at B's clock even while B is held.

## Steps

1. **A test program (done, see above: it runs, 4.8x slower):** core A runs code copied to core B's RAM. If that fails, the code has to fit
   in core A's own memory (about 270 KiB with everything taken), so the tables shrink
   (`FAST_CPU_SMALL_FLASH`) and the music goes.
2. **Loader, API version 2:** a "big" program asks for the extra memory. The loader then:
   - holds core B and stops the audio DMA
   - gives the program those ranges
   - when the program ends, reloads the fonts and restarts core B
3. **Port the game (done, device 2026-11-26: E1M1 plays):** `ports/doom/`, its README has the
   details. In short:
   - the screen: rendered natively at 320×170 (3D view 320×138, the full status bar), no crop or
     scale; the 320×200 pictures (menus, title) come out at 85 % height
   - the keys: walking is a toggle (PLAY), since the ladder reads one key at a time
   - the WAD: `GILB/APPS/DOOM.WAD` (converted with `MCUDoomWadUtil`), read in 1 KiB pages, 4 per request
   - the MG24's internal-flash cache (level data, flats, textures): core B's upper 252 KiB
   - no BLE, no sound
4. **Sound (done, device 2026-11-26: the effects play):** API version 5 (`sound_open`, a ring the
   program fills, the audio DMA looping over it; docs/PROGRAMS.md) and Doom's mixer at 44100 Hz
   (ports/doom/README.md, "Sound"). Music next.

## What was built (2026-11-26)

- **API version 3:** `more_mem_a` lends core A's fonts window, the audio DMA list and the PCM ring
  (0x03077900..0x030A0000, 161.75 KiB). It stops the audio DMA (`audio_pcm_free`); GILB loads its
  fonts again when the program ends. Resident cost: a few bytes of code, no RAM.
- **Two files.** GILB loads only `DOOM.APP` (112 KiB at most), so a first stage in it reads
  `DOOM.BIN` (the other blocks) after asking for the memory.
- **Where it went** (`ports/doom/doom.ld`):

  | Block | Used for |
  |---|---|
  | program region, 112 KiB | the hot code: 107 KiB |
  | fonts window + PCM ring, 161.75 KiB | the smaller math tables (30.6 KiB, `FAST_CPU_SMALL_FLASH`), the cold code (50 KiB), the 8-bit frame, the RGB565 bands, 20 KiB of WAD pages |
  | core B 0x01000000, 128 KiB | `.data`, `.bss`, `info.c` (the game's 16-bit pointers reach only this window), the zone |
  | core B 0x01021000, 252 KiB | the level cache |

  No code runs from core B's RAM.
- **E1M1 on the device:** 58.6 KiB of fixed data plus 188 KiB of level data and textures, out of
  254 KiB; 44 KiB of the zone free. It loads in 6 s.
- **Speed:** 24 to 34 fps in plain rooms, 8 to 15 in busy ones. Each storage request costs about
  1.1 ms (4 KiB), and in busy rooms the textures that didn't fit the cache and the sprites take
  25 to 45 % of the time. The rest is core A at 96 MHz.

Bugs found on the way:

- The MG24 port's `I_Error` only printed, and the game goes on after it (`HELP` is missing in the
  shareware WAD, so it tries `HELP1`).
- `fastOpeningsCopy` copied 2-aligned shorts as words; GCC made that `ldrd`, which faults on a
  Cortex-M3.
- A flat that didn't fit the cache stopped the renderer; it's now read from the WAD.
- GILB's own: `a_poll` stored POWER's press time as `now | 1`, so an unsigned `now - t0` read
  as "held 2 s" right away on even milliseconds, and a tap left any program (fixed in
  `services/programs.c`).

## Still to do

- **Speed:** core A at a faster clock while a program runs (the FLAC point, A at 224 MHz, which
  `services/cpuclk.c` gates behind its own self-test), and fewer storage requests.
- **Memory:** E1M6's level data (about 220 KiB) won't fit next to the fixed data. It stops with
  "too big for the player's memory". Not checked on the device yet; E1M2 to E1M9 are untested.
- **Music: done 2026-11-27 (device):** not live (`opl2.c` is 57 KB and much of core A's time): each
  song is rendered once on the PC with the port's own synthesis (`ports/doom/tools/music`) and
  streamed from `GILB/APPS/DOOMMUS` (ports/doom/README.md, "Music").
- **Save games:** write `DOOM.SAV` / `DOOM.CFG` in `GILB/APPS`; untested.
