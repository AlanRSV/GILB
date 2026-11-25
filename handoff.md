# GILB handoff (2026-11-25)

Start here in a new session. This file covers where the project stands, how the user works, and how to build,
install and check changes. (`docs/HANDOVER.md` is an older history from October 2026. Don't use it for the current state.)

## The project

- **GILB** is a custom firmware for the SNOWSKY Echo Mini music player. It is a fork of ReChord,
  which is itself a decompile of RECHO, the first custom firmware.
  - Local folder: `C:\Users\loy\Documents\GitHub\ReChord`. GitHub: **AlanRSV/GILB** (`origin`).
  - Internal identifiers (`RECHORD_*` macros, `rechord_app.c` and so on) keep the old name on purpose.
- **Echo Library** is the companion web app.
  - Local folder: `C:\Users\loy\Documents\GitHub\musicorganizer`. GitHub: AlanRSV/musicorganizer. It runs on Railway.
  - It manages the library, covers and tags, and has a Themes page that writes GILB theme files.
  - Run the tests in WSL with Node from `~/.local/node/bin` (not on the PATH): a script that sets
    `PATH="$HOME/.local/node/bin:$PATH"`, cds to `/mnt/c/Users/loy/Documents/GitHub/musicorganizer` and runs
    `npm test`; 90 pass. `.claude/launch.json` in this repo starts its dev server the same way (its script path
    points at an old scratchpad: update it if the server won't start).
  - The dev server is launch config `echo-library` (port 5173) in `.claude/launch.json`. Restart it to see file changes.
- **Hardware:** the user owns two Echo Minis, both the **8 GB board**. One runs GILB. The other is still on
  stock 4.0.0 (drive `D:` in USB disk mode), and the user plans to flash it.
  - The user's music is on a 128 GB microSD card. Through the card reader it shows up as drive `G:`.
  - In USB disk mode the GILB player's internal storage is drive `F:` (check the label and the `GILB` folder:
    letters can change).
  - `GILB/CJK.BIN` (Japanese/Chinese/Korean glyphs) is on the GILB player; a copy is in `build/CJK.BIN`.

## How the user works

- The user picks the next item from the to-do list ("What's next on the list?"), tests each build
  on the player, and replies "Looks right", "All works" and so on.
- **Commit straight to `main`, and only when the user says "Commit and push."** End every commit message with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Always build with **`REVERSE_LISTS=1`**, because the user wants VOL- to move up in lists.
- Never send the user's name, email or project details to outside services.
- Don't run downloaded installer scripts; for example, get-pip.py was refused. Write the tool yourself instead.
- List `F:\` before saying which files are on the player. Windows won't let Claude delete files on `F:`, so ask the user to do it.
- Keep explanations plain and short; the user asks when they want more detail.

## Build (in WSL; the repo is at `/mnt/c/Users/loy/Documents/GitHub/ReChord`)

Build core A:

```
make cfw-ap BOARD=8g REVERSE_LISTS=1
```

Build the image to flash (`build/HIFIEC90.IMG`):

```
make cfw-img BOARD=8g REVERSE_LISTS=1 STOCK_IMG='stock/4.0.0/ECHO MINI V4.0.0/HIFIEC40.IMG' REF_IMG='community/refcfw/RECHO-0.10.1/HIFIEC90.IMG' FONT_IMG='stock/4.0.0/ECHO MINI V4.0.0/HIFIEC40.IMG'
```

Build the desktop simulator (`build/sim/rechord_sim_8g`), which runs the firmware's own UI code:

```
make sim BOARD=8g REVERSE_LISTS=1
```

Run the host tests:

```
make host-tests
```

`run_tags.sh` fails in WSL because of CRLF line endings and because `lame` isn't installed. This is a known environment issue, not a code bug.
It also stops `make host-tests` before the theme test, so run that one directly:

```
cc -std=c99 -Wall -Werror -Ifirmware/theme -o build/host/test_theme firmware/theme/tests/test_theme.c firmware/theme/theme.c firmware/theme/themes_builtin.c firmware/theme/theme_file.c && build/host/test_theme
```

**Pitfalls:**

- After a header change or a flag change, objects can go stale (an mtime issue in WSL). Rebuild clean with
  `rm -rf build/cfw` or `rm -rf build/sim/obj-8g`.
  - Switching LTO on or off always needs a clean build.
  - If make reports clock skew, run `wsl --shutdown` first.
- Bash heredocs halve backslashes. For code with backslashes, use the Edit tool or a Python script in the scratchpad,
  run with `MSYS_NO_PATHCONV=1 wsl -e python3 -I /mnt/c/...`.
- `/tmp` in Git Bash is not the same as `/tmp` in WSL.

**Install:**

1. With GILB running, plug in the cable and pick **USB disk**.
2. Copy `build/HIFIEC90.IMG` to the root of `F:\`.
3. The user ejects the drive, confirms on the player (it shows the SHA-256), and presses PLAY.

If GILB won't start, the player shows "USB recovery (boot ROM)". Ask the user to photograph the screen
*before* plugging in. Recovery steps are in `docs/FLASHING.md` and `docs/DEBRICK-LINUX.md`.

## Memory: the main constraint

- Core A's resident code must leave at least **3072 B free** (`AP_MIN_FREE`, lowered from 4096 on 2026-11-25,
  the user's choice, for layouts phase 2); otherwise the link fails. The 8 GB build has **4488 B free** now,
  1416 B above the floor (RAM: 948 B free) (`build/cfw/ap_size.txt` shows the numbers). `LTO=0` builds need
  `AP_MIN_FREE=0`.
- New features go into **overlays**: swap-in code windows, opened through gates in `firmware/startup/ap_ovl.c`
  (`GATE`, `npx_ok()`). Files wholly in an overlay are placed by object name in `firmware/firmware_ap.ld`;
  mostly resident files tag their overlay functions `OVL_TEXT(<overlay>)` (`services/ovl.h`), so they can
  still use LTO. `NOCROSSREFS` stops resident code calling into an overlay. Room left:
  - modes window (14 KB): viz about 2.8 KB free, npx about 0.8 KB, lay (layouts, and `config_load`) about 3.4 KB, usbdac 6.7 KB,
    usbout 4.9 KB, dev 1 KB.
  - fonts window: about 3.6 KB, used as the CJK glyph cache.
  - lib window: about 390 B.
- LTO: the link compiles each function apart (`-flto-partition=max`) so a string shared by resident and
  overlay code stays resident. `ap_startup.o`, `watchdog.o` and `bootguard.o` stay out of LTO (inline asm;
  the `.noinit` crash record at `0x0302FFC8`).
- `docs/MEMORY_MAP.md` has the full memory layout.

## Where things are

| Area | Files |
|---|---|
| Settings file (`GILB.CFG`) | `firmware/services/config.c/.h`, `g_cfg` |
| One-time move of `RECHORD/`, `RECHORD.CFG` to `GILB/` | `firmware/services/gilb_move.c` |
| Themes: Modern built-ins Classic, Paper, Night, Contrast, Pink plus `GILB/THEMES/*.txt`; Retro's own built-ins LCD, Amber, Green LCD, Mono plus `GILB/THEMES/RETRO/*.txt` (three colours: glass, ink, highlight) | `firmware/theme/theme_store.c`, `theme.c` (two name lists, `theme_set_look`), `themes_builtin.c` (`theme_retro_derive`); `GILB.CFG` `theme=` and `retro_theme=`; Echo Library `src/themes.js` (`retroDerive`), `src/themepreview.js`, the Themes page's Modern / Retro switch in `src/main.js` |
| Look: Modern or Retro (Settings › Display › Look) | `g_cfg.look`, `theme_apply()`; Now Playing from layouts (below); year lookup in `ui/retro_tags.c` (lay overlay); header and lists from layouts (below); grid in `nav_grid_draw` |
| Fonts (blob `RCFONT2`) | `firmware/drivers/fonts/rechord_fonts.bin` and the generated `fonts_blob.c`; `drivers/text.c` (`F_PX`) |
| GILB's own pixel font PX (ASCII, curly quotes, dashes) | `tools/gilb_pixfont.py`: edit a glyph's rows, then run `blob`; `look_font()` in `ui/frame.c` picks PX in Retro |
| Japanese/Chinese/Korean glyphs | `tools/gilb_cjkfont.py` builds `GILB/CJK.BIN` (Fusion Pixel 12 px, Japanese style); `services/cjk_font.c`, cache in `drivers/text.c` |
| Layouts (docs/LAYOUTS.md): Now Playing (`[np]`), list rows (`[list]`) and the header (`[header]`) from `GILB/LAYOUTS/*.txt`, Settings › Display › Layout per Look | `ui/layout.c` (lay overlay: parser, Now Playing's drawing, built-in Modern and Retro layouts, also `config_load`), `ui/layout.h`; parsed into `g_lay`, `g_lrow`, `g_lhdr` (`ui/screens.c`) by `ap_lay_load()`; rows drawn by `draw_row` in `ui/nav.c`, the header by `hdr_pass` in `ui/frame.c` (both resident); `GILB.CFG` `layout=`, `retro_layout=`; Echo Library `src/layouts.js` (a test checks its built-ins against `ui/layout.c`) and Themes › Layouts in `src/main.js` (Now Playing, Lists, Header) |
| Simulator screenshots | `sim/README.md`: `--headless --script FILE` presses keys and saves BMPs |
| Library views (Albums grid, Years, ...) | `firmware/ui/nav.c`; index build in `lib_build.c` (index version 3) |
| Self-test (Settings › System) | `firmware/services/selftest.c` (dev overlay) |
| Screens and settings menus | `firmware/ui/screens.c` |
| Feature list and device-test status | `docs/ROADMAP.md` |
| Hardware decoding results | `docs/HIFI_PLAN.md` §4.5 |

## State of the to-do list

Everything is committed and pushed. The last firmware change is **2fc8d39**; Echo Library's is **13d227a**.

Done and confirmed on the device:

- Years view, hold-to-boot, self-test, themes (Pink plus theme files), the GILB rename
- full LTO (a24b054): 8 GB build went from 4908 to 6548 B free before the later features
- Japanese / Chinese / Korean text from `GILB/CJK.BIN` (9b058de)
- Retro look on every screen: lists, menus, grid, header, Now Playing and its Lyrics / Up Next / Spectrum
  views, questions, sliders, info pages, Custom EQ, Rename, USB DAC (a24b054, c2c37fc, f3f2562), and Now
  Playing's darker panel in a light rim (9764150)
- Layouts phase 1 (30eefd3; Echo Library fe358c2): Now Playing drawn from layout files, edited in Echo Library
  (drag, resize, add); Echo Library's colours from a picture (0cb2e31)
- Layouts phase 2 (2fc8d39; Echo Library 13d227a): every list's rows and the header drawn from layout files,
  edited in Echo Library; Retro's hand-written lists removed
- Retro's own themes (0b51e63; Echo Library 3d1156c): Settings › Display › Theme lists the current Look's
  themes, each Look remembers its own, and Echo Library's Themes page edits both
- the self-test's audio step: 4 s a side, 2 s gap; its test feed now writes one 32-bit word a channel
  (b741491). The DAC chips' channel values stay as stock's: b741491's swap was reverted in e2cab7d (the
  earphones in that test were in backwards). `LR TEST/LR test.flac` (`build/lrtest/`) checks left/right.

The README has new Modern (Pink) and Retro screenshots and the corrected credits (RECHO's creator first).

Dropped or set aside:

- **Skipped or no change needed:** the Bluetooth reconnect edge case (never seen), Composers view, USB control
  channel, faster copying, crash-safe settings (already safe), key timing step 2, screen tearing.
- **Hardware decoding:** closed. The FFT output can't be read back. The FLAC block stays on.

Still open:

- **Layouts phase 3** (docs/LAYOUTS.md): questions, sliders, info pages, Lyrics, Up Next, Spectrum, the EQ
  editor, Rename, the USB DAC screen. Resident code has 1416 B above the floor, so their drawing should stay
  in overlays (lay has about 3.4 KB left) or replace the hand-drawn code it takes over.

- **Second Echo Mini:** flash it (`docs/FLASHING.md`), run the self-test, copy `GILB/CJK.BIN` to it.

The list is now driven by the user's own ideas. Ask what they want next.
