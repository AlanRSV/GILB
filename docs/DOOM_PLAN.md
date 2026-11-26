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
3. **Port the game:**
   - replace `src/` with GILB's API: the screen as 320×170 (the port renders 320×200 or 320×240)
   - map the 7 keys
   - read the WAD from a file
   - keep the texture cache in RAM
   - no BLE, no sound
4. **Then:** sound, through a new API call that feeds GILB's PCM output.
