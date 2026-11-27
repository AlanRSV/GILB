# GILB program API reference

Everything a program gets from GILB, function by function. The source of truth is
[`include/gilb_app.h`](include/gilb_app.h); [`README.md`](README.md) has how to build a program, and
[`../docs/PROGRAMS.md`](../docs/PROGRAMS.md) how the loader works inside the firmware.

This page is for API **version 6** (GILB built with `PROGRAMS=1`, 2026-11-27).

## A program's shape

```c
#include "gilb_app.h"

static uint16_t band[2][320 * 16];          /* two buffers: see blit */

int app_main(const gilb_api_t *api)
{
    for (int frame = 0;; frame++) {
        gilb_key_t ev;

        if (api->poll())                    /* POWER held 2 s: leave */
            return 0;
        while (api->key(&ev))
            if (ev.key == GK_BACK && ev.gesture == GE_HOLD)
                return 0;
        /* ... draw into band[frame & 1], then: */
        api->blit(0, 0, 320, 16, band[frame & 1], 320);
    }
}
```

GILB calls `app_main` once. Return from it to end the program; GILB logs the value you return
(`programs: NAME.APP ended (0)` in `ECHOLOG.TXT`) and does nothing else with it.

### When it runs

- The music has stopped, and the screen and buttons are the program's.
- Its memory, `0x03030000`..`0x0304C000` (112 KiB), holds its code, data and `.bss`. `.bss` is zeroed
  before `app_main`.
- The stack is GILB's: keep big arrays `static`, not local. About 10 KB of stack is safe.
- Afterwards GILB is back where it was, with no restart: its menus, the library and the music work
  as before.

## Version and board

| Field | What it is |
|---|---|
| `version` | The API version of this firmware (4 today). |
| `api_size` | `sizeof(gilb_api_t)` in this firmware. |
| `board` | 0 = the 8 GB board, 1 = the 512 MB board. |
| `screen_w`, `screen_h` | 320 and 170. |
| `dir` | `"/Internal/GILB/APPS"`, the folder the program's file is in. |

**Using a newer function safely:** functions are only ever added at the end of `gilb_api_t`. A
program that needs version N either builds with `API_MIN=N`, so an older GILB refuses it with
"Needs a newer GILB", or checks before calling:

```c
if (api->version >= 3 && api->api_size >= sizeof(gilb_api_t))
    /* api->more_mem_a is there */
```

## The screen

The panel is 320 × 170, RGB565 (5 bits red, 6 green, 5 blue), with (0, 0) at the top left. There's
no frame buffer in GILB: a program sends rectangles of pixels.

### `void blit(int x, int y, int w, int h, const uint16_t *px, int pitch)`

Sends `w` × `h` pixels to the screen at (`x`, `y`). `px` points at the first pixel; rows are `pitch`
pixels apart (usually `pitch` = `w`). Parts off the screen are clipped.

The transfer runs in the background and may still be reading `px` after `blit` returns. Don't change
those pixels until the next `blit` or a `sync()`. The simplest way is two buffers used in turn, as
in the example above. A program that redraws one buffer straight away gets a garbled screen.

Drawing in bands of 16 rows is quick and needs only 10 KB a buffer. Hello reaches about 60 frames a
second redrawing the whole screen that way.

### `void sync(void)`

Waits until the last `blit` has finished reading its pixels.

### `void backlight(int percent)`

Sets the screen's brightness, 0 (off) to 100. GILB puts the user's own brightness back when the
program ends.

### `const uint8_t *glyph(int ch)`

GILB's 8 × 8 font: 8 bytes, one per row from the top, with bit 7 as the leftmost pixel. It covers
`' '`..`'~'`; anything else gives `'?'`. Draw text by setting the pixels whose bits are 1.

## Buttons

### `int key(gilb_key_t *ev)`

Takes the oldest button event into `*ev` and returns 1, or returns 0 when there's none. Read them
all each frame (`while (api->key(&ev))`).

| `ev` field | Meaning |
|---|---|
| `key` | `GK_VOLUP`, `GK_VOLDOWN`, `GK_PLAY`, `GK_BACK`, `GK_POWER`, `GK_MENU`, or `GK_CHORD` (VOL+ and VOL− together). |
| `gesture` | What happened: see below. |
| `taps` | For `GE_TAP` and `GE_END`: 1, 2 or 3 (single, double, triple tap). |
| `steps` | For `GE_REPEAT`: how many steps to move now. |
| `t_ms` | When it happened, in `ms()` time. |

The gestures, in the order a press makes them:

| Gesture | When |
|---|---|
| `GE_DOWN` | The button went down. For a game, it's held from here... |
| `GE_TAP` | ...released quickly (or to here). |
| `GE_HOLD` | Still held after 0.5 s. |
| `GE_REPEAT` | VOL+ or VOL− still held: move by `steps`. |
| `GE_RELEASE` | Released after a `GE_HOLD` or `GE_REPEAT` (or to here). |
| `GE_END` | A double or triple tap finished; `taps` says how many. |
| `GE_CHORD` | Two buttons together. |

For a game, treat `GE_DOWN` as "pressed" and `GE_TAP` or `GE_RELEASE` as "let go".

**Leaving:** holding POWER for 2 seconds always works (see `poll`). Programs should also leave on
their own key, such as BACK held.

## Time and keeping GILB going

### `int poll(void)`

Call it every frame, or at least every 100 ms. It keeps the player's own work going: the buttons,
queued file writes, the log, Bluetooth, the USB serial port and the watchdog. Without it the
buttons stop and, after a while, the player resets.

Returns 1 when POWER has been held for 2 seconds: the user wants out, so return from `app_main`.

### `uint32_t ms(void)`

Milliseconds since power-on.

### `void sleep_ms(uint32_t ms)`

Waits `ms` milliseconds, calling `poll` meanwhile. It doesn't report POWER being held, so check
`poll()` afterwards in a long wait.

## Files

Paths start with `/Internal/` (the internal storage) or `/SD/` (the microSD card), for example
`"/Internal/GILB/APPS/HELLO.CNT"`. `api->dir` is the program's own folder.

| Function | What it does | Returns |
|---|---|---|
| `void *open(const char *path, int mode)` | Opens a file. `GF_READ`; `GF_WRITE` (creates it, or empties it); `GF_APPEND`. | A handle, or `NULL` (no such file, no room, or a bad name). |
| `long read(void *f, void *buf, unsigned long n)` | Reads up to `n` bytes. | Bytes read (0 at the end), or < 0. |
| `long write(void *f, const void *buf, unsigned long n)` | Writes `n` bytes. | Bytes written, or < 0. |
| `long seek(void *f, long offset)` | Moves to `offset` bytes from the start. | 0, or < 0. |
| `long size(void *f)` | The file's length. | Bytes, or < 0. |
| `int close(void *f)` | Closes it. | 0, or < 0. |
| `int exists(const char *path)` | Is the file there? | 1 or 0. |

- New files on the internal storage need 8.3 names (`NAME.EXT`) while a program runs.
- Close files before returning from `app_main`.
- Writes to the storage are queued, and the queue moves while you call `poll`.

## The log

### `void log(const char *line)`

Adds a line to GILB's log, `ECHOLOG.TXT` on the internal storage, as `app: <line>`. The log is the
easiest way to see what a program did after the fact.

## More memory (versions 2 and 3)

A program's own 112 KiB is enough for most things. Two calls lend more. The lent memory's
contents are undefined, and it's the program's until it ends.

### `int more_mem(gilb_mem_t *out, int max)` (version 2)

Lends the audio core's RAM: 380 KiB in two ranges, `0x01000000` (128 KiB) and `0x01021000`
(252 KiB). Fills up to `max` ranges into `out` and returns how many.

- The audio core is stopped and loads again at the next play.
- Never write to `0x01020000`..`0x01021000`, between the two ranges: GILB keeps a shared block there.
- It's slower than the program's own memory. Measured on the player: data is about 1.6× slower,
  and code run from it about 4.8× slower. Keep hot code and data in the program's own memory.

### `int more_mem_a(gilb_mem_t *out, int max)` (version 3)

Lends 161.75 KiB of the main core's own fast RAM, one range, `0x03077900`..`0x030A0000`: GILB's
fonts window and the audio buffers. Fills up to `max` ranges into `out` and returns how many.

- The music's audio output stops; the next play sets it up again.
- GILB can draw only its 8 × 8 font until the program ends. It loads its fonts again afterwards.

`ports/doom/` uses both: its hot code and data in the program's memory and this range, its cold
code and the game's data in the audio core's RAM.

## USB serial port (version 4)

With the USB cable in, the player can be a serial (COM) port on the PC. Windows uses its own
driver. It's for a PC keyboard, debugging, or anything else a program wants to exchange with a PC.

| Function | What it does | Returns |
|---|---|---|
| `int serial_open(void)` | Turns the port on. | 0; −1 with no cable; −2 while USB-C earphones are in. |
| `int serial_ready(void)` | Has the PC set the port up? | 1 or 0. |
| `long serial_read(void *buf, unsigned long n)` | The bytes the PC sent, up to `n`. | Bytes read (0: none yet), or −1 if the port isn't open. |
| `long serial_write(const void *buf, unsigned long n)` | Sends `n` bytes to the PC. | Bytes sent, or −1 if the port isn't open. |

- The PC's requests are answered inside `poll()`, so call it each frame.
- GILB closes the port when the program ends; the PC sees it go away.
- The port's USB ID is `071B:3209`.
- [`../tools/gilb_keys.ps1`](../tools/gilb_keys.ps1) sends a PC's keys: one byte for each change,
  the Windows key code, with bit 7 set for a release. Doom reads it that way.

## Sound (version 5)

Sound out of the headphone jack. The program owns a **ring** of sound frames in its own memory and
keeps writing into it ahead of where the player is; GILB's audio DMA plays it round and round.

| Function | What it does | Returns |
|---|---|---|
| `int sound_open(uint32_t rate, int32_t *ring, uint32_t block_frames, uint32_t blocks)` | Stops the music, turns the DAC on at `rate`, clears the ring and starts playing it in a loop. | 0; −1 a bad ring or rate; −2 the DAC didn't come up. |
| `uint32_t sound_pos(void)` | The frame being played now, 0 .. `blocks` × `block_frames` − 1. | The frame. |
| `void sound_close(void)` | Stops the sound. GILB also does this when the program ends. | |

- **Rates:** 32000, 44100, 48000, 88200, 96000, 176400 or 192000 Hz. The DAC (a CS43131) plays
  nothing below 32 kHz, so a program with older 11025 Hz sounds, like Doom, plays them at 44100 Hz,
  four frames a sample.
- **A frame** is two `int32_t` words, left then right, with the sample in the top bits: a 16-bit
  sample `s` is `(int32_t)s << 16`.
- **The ring** is `blocks` × `block_frames` frames: 2 to 16 blocks of 16 to 4096 frames, 8-byte
  aligned, in the program's own memory (its 112 KiB, or what `more_mem_a` lent). The DMA reads core
  A's RAM only, so not `more_mem`'s.
- **Keeping it fed:** write the frames from a little after `sound_pos()` up to just before it comes
  round again, and do that often (each `poll()`, during long jobs too). The DMA never waits: what it
  finds is what plays, so a ring left alone repeats its old sound.
- **Volume:** the player's own volume.
- **USB:** `serial_open` switches the DAC off as USB starts, and GILB turns it on again: a short
  click, and the ring starts silent.

```c
#define RATE 44100u
#define BLOCK 256u
#define BLOCKS 8u                           /* 2048 frames: 46 ms */
static int32_t ring[BLOCKS * BLOCK * 2] __attribute__((aligned(8)));

api->sound_open(RATE, ring, BLOCK, BLOCKS);
for (;;) {
    uint32_t pos = api->sound_pos(), n = BLOCKS * BLOCK;
    /* write the frames (pos + 256) .. (pos - 16), modulo n: */
    for (uint32_t k = 256; k < n - 16; k++) {
        uint32_t i = (pos + k) % n;
        int16_t s = next_sample();          /* (the program's own) */
        ring[2 * i] = ring[2 * i + 1] = (int32_t)s << 16;
    }
    if (api->poll())
        break;
}
```

That example writes everything again each time, which is simplest. Doom's mixer
([`../ports/doom/gilb/gilb_sound.c`](../ports/doom/gilb/gilb_sound.c)) does the same and keeps
its own count of the frames played, so it carries on correctly after a long frame.

## A faster CPU (version 6)

| Function | What it does | Returns |
|---|---|---|
| `int fast(int on)` | `fast(1)`: core A runs at 224 MHz instead of 96 while the program runs. `fast(0)`, or the program's end, goes back. | 0; < 0 not now (GILB's log says why). |

- **Call it after `more_mem`** (core B must be off: −5 otherwise) **and before `sound_open`.**
- **The first time a boot**, GILB tests the CPU at the new clock (about 20 ms). If anything fails, it
  stays at 96 MHz, and so does every later call with that GILB build. A player that resets during
  the switch keeps it off for that build too.
- Storage, the screen and the sound keep their own clocks: only the program's code runs faster.
  Doom went from 3 to 13 fps in E1M1 to 13 to 23.
- It uses more battery while on.

```c
if (api->version >= 6 && api->api_size >= sizeof(gilb_api_t))
    api->fast(1);                           /* after more_mem */
```

## Not in the API yet

- **Sound input**, and sound out of a USB-C DAC or Bluetooth (programs use the headphone jack).
- **A C library.** `crt0.c` has `memset`, `memcpy`, `memmove` and `memcmp` only. There's no
  `malloc` or `printf`: use static arrays, and bring a small freestanding library if you need more.
- **Floating point in hardware.** The CPU is a Cortex-M3, so `float` works but is slow. Use
  integers or fixed point.
- **The clock, Bluetooth and the music library.**

## The file format

A `.APP` file is the program's memory image from `0x03030000`, starting with this 64-byte header.
The SDK makes it (`crt0.c` fills it in, `tools/gilb_app.py` sets the CRC), so you don't need to.

| Offset | Field | Value |
|---|---|---|
| 0 | `magic` | `0x50504147` ("GAPP") |
| 4 | `hdr_size` | 64 |
| 8 | `api_min` | the API version it needs |
| 12 | `base` | `0x03030000` |
| 16 | `load_end` | `base` + the file's length |
| 20 | `mem_end` | the end of its `.bss` (`load_end`..`mem_end` is zeroed) |
| 24 | `entry` | the address of `app_main` (odd: Thumb code) |
| 28 | `crc` | CRC-32 of the file with this field as 0 (polynomial `0x04C10DB7`, MSB first, start 0) |
| 32 | `name` | the name in the list (32 bytes, UTF-8, NUL-terminated; empty: the file's name) |

GILB checks all of these before running a program, and shows why it refused one: "Not a GILB
program", "Needs a newer GILB", "Built for another address", "Damaged (length)", "Too big for the
player", "Damaged (entry)" or "Damaged (CRC)".
