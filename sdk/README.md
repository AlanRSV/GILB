# GILB SDK: programs for the Echo Mini

A **program** is a `.APP` file that GILB runs from **Settings › System › Programs**. It gets the
whole screen, the buttons, files and 112 KB of memory. When it ends, the player is back in GILB
where it was, with no restart.

## Build the example

The tools are the firmware's own: `arm-none-eabi-gcc` and `python3`, in WSL, Linux or macOS.

```sh
make -C sdk APP=hello NAME=Hello
```

That writes `sdk/build/HELLO.APP`. To put it on the player:

1. Copy it to the `GILB/APPS` folder on the player's internal storage. Make the folder if it isn't there.
2. On the player, open Settings › System › Programs and pick **Hello**.

Hello draws its own screen, bounces a square, shows the last button press and the frame rate, and
counts its runs in `GILB/APPS/HELLO.CNT`. Hold BACK to leave it, or hold POWER for 2 seconds, which
leaves any program.

## Write your own

1. Copy `examples/hello` to `examples/<name>`.
2. Write `int app_main(const gilb_api_t *api)` and return from it when the program is done.
3. Build it with `make -C sdk APP=<name> NAME="Shown name"`. The file's name is the folder's
   name: 8 characters at most, in capitals, plus `.APP`.

[`include/gilb_app.h`](include/gilb_app.h) lists everything `api` offers:

- **Screen:** `blit` (RGB565 pixels to any rectangle), `sync`, `backlight`, and `glyph` (GILB's
  8×8 font). The screen is 320 × 170.
- **Buttons:** `key`. Events say which button and what happened to it: down, tap (with double
  and triple taps), hold, repeat or release.
- **Time:** `ms` and `sleep_ms`.
- **Files:** `open`, `read`, `write`, `seek`, `size`, `close` and `exists`, on `/Internal/...`
  or `/SD/...`. New files on the internal storage need 8.3 names (`NAME.EXT`) while a program runs.
- **Logging:** `log` writes a line to GILB's log (`ECHOLOG.TXT`).
- **`poll`:** call it every frame, or at least every 100 ms. It keeps the player's own work going,
  and it returns 1 when the user holds POWER to leave.

## The rules

- **Memory:** everything is linked at `0x03030000`. Code, data and `.bss` share 112 KB, and the
  build stops if a program doesn't fit. The stack is GILB's (about 10 KB is safe). There's no
  `malloc`: use static arrays.
- **What's missing:** there's no C library. `crt0.c` has `memset`, `memcpy`, `memmove` and `memcmp`.
  Write the rest yourself or bring a small freestanding one.
- **No floating-point hardware** (Cortex-M3). Use integers or fixed point.
- **Sound** isn't in the API yet. It's next, for the Doom port.
- **Compatibility:** a program states the API version it needs (`APP_API_MIN`, default 1). GILB
  refuses a program that needs a newer one. Newer GILBs only add functions at the end of
  `gilb_api_t`, so older programs keep working.

[`docs/PROGRAMS.md`](../docs/PROGRAMS.md) has how the loader works inside the firmware.
