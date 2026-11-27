# GILB handoff (2026-11-27)

Start here in a new session, and read this file again after every context compaction (CLAUDE.md). This file covers where the project stands, how the user works, and how to build,
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
- **Programs are a beta** (the user's ask, 2026-11-26): only images built with **`PROGRAMS=1`** have
  Settings › System › Programs. Build the user's own test images with it; normal and release builds leave it out.
- Publish nothing (GitHub releases, pre-releases) without the user's yes in the chat.
- Never send the user's name, email or project details to outside services.
- Don't run downloaded installer scripts; for example, get-pip.py was refused. Write the tool yourself instead.
- List `F:\` before saying which files are on the player. Windows won't let Claude delete files on `F:`, so ask the user to do it.
- Keep explanations plain and short; the user asks when they want more detail.

## Build (in WSL; the repo is at `/mnt/c/Users/loy/Documents/GitHub/ReChord`)

The repo holds only today's code: `firmware/` (core A: `ap.mk` lists its sources; core B: `coreb/`;
`compat/`: the two SDK headers every core-A file force-includes), `sim/`, `tools/`, `docs/`. Everything
from the earlier stages (the QEMU harness, the Rockchip SDK and its `make release` pipeline, the boot
experiments, the first player and module system, the reverse-engineering tools) is in `archive/`, with
its history; `archive/README.md` says what each was. Plain `make` builds core A.

Build core A:

```
make cfw-ap BOARD=8g REVERSE_LISTS=1
```

Build the image to flash (`build/HIFIEC90.IMG`):

```
make cfw-img BOARD=8g REVERSE_LISTS=1 STOCK_IMG='stock/4.0.0/ECHO MINI V4.0.0/HIFIEC40.IMG'
```

No `REF_IMG` (since 2026-11-25): `tools/make_fw1.py` writes the update image's outer header itself
(`write_outer_header`, `docs/fw1-packing.md` §11); it no longer copies RECHO's.

No `FONT_IMG` (since 2026-11-25): it copied Snowsky's stock bitmap fonts (about 2.1 MB) into the image, and
nothing in the firmware reads them; an image without them was tested on the device (all text as before).
Images for others (the download site, `site/`) must leave them out. Japanese, Chinese and Korean glyphs
are `GILB/CJK.BIN` (Fusion Pixel, SIL OFL 1.1): attach it to a release as an optional file.

A beta image with Programs (the user's test images, and early betas):

```
make cfw-img BOARD=8g REVERSE_LISTS=1 PROGRAMS=1 STOCK_IMG='stock/4.0.0/ECHO MINI V4.0.0/HIFIEC40.IMG'
```

Switching `PROGRAMS` (like `BOARD` or `REVERSE_LISTS`) rebuilds every object (a stamp file in `build/cfw`).

Programs and Doom (in WSL):

```
make -C sdk APP=hello NAME=Hello        # sdk/build/HELLO.APP
make -C ports/doom                      # ports/doom/build/DOOM.APP and DOOM.BIN
make -C ports/doom/tools/music          # the Doom song renderer (a PC program)
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
It also stops `make host-tests` before the tests after it (display, theme, dirty rects, fs_write, power cut,
64-bit division), so run those directly, for example:

```
cc -std=c99 -Wall -Werror -Ifirmware/theme -o build/host/test_theme firmware/theme/tests/test_theme.c firmware/theme/theme.c firmware/theme/themes_builtin.c firmware/theme/theme_file.c && build/host/test_theme
cc -O2 -Wall -Werror -D__udivmoddi4=my_udivmoddi4 -o build/host/test_udiv64 firmware/libc/tests/test_udiv64.c firmware/libc/udiv64.c && build/host/test_udiv64
```

Known failures that are not code bugs (all fail the same on older commits): `firmware/drivers/tests/run_tests.sh`
has CRLF line endings (run a copy with them stripped); its `test_text` fails `text_fonts_init == 4` (the test
expects an older font count) and, in the Windows checkout only, two PPM goldens (line endings);
`tools/test_fs_write.py` is killed in WSL (out of memory).

**Pitfalls:**

- After a header change or a flag change, objects can go stale (an mtime issue in WSL). Rebuild clean with
  `rm -rf build/cfw` or `rm -rf build/sim/obj-8g`.
  - Switching LTO on or off always needs a clean build.
  - If make reports clock skew, run `wsl --shutdown` first.
- Bash heredocs halve backslashes. For code with backslashes, use the Edit tool or a Python script in the scratchpad,
  run with `MSYS_NO_PATHCONV=1 wsl -e python3 -I /mnt/c/...`.
- `/tmp` in Git Bash is not the same as `/tmp` in WSL.
- Git Bash rewrites `/mnt/...` arguments into Windows paths: run WSL tools as
  `wsl -e bash -lc '...'` (or with `MSYS_NO_PATHCONV=1`).
- WSL doesn't see the player's drive (`F:`): copy to it from Git Bash (`/f/...`).
- No Node.js on this PC or in WSL (Echo Library's tests use `~/.local/node`): to try `site/` locally, a
  small Python stand-in that serves `site/public` and the test JSON works (`site/README.md`, mocks).

**Install:**

1. With GILB running, plug in the cable and pick **USB disk**.
2. Copy `build/HIFIEC90.IMG` to the root of `F:\`.
3. The user ejects the drive, confirms on the player (it shows the SHA-256), and presses PLAY.

If GILB won't start, the player shows "USB recovery (boot ROM)". Ask the user to photograph the screen
*before* plugging in. Recovery steps are in `docs/FLASHING.md` and `docs/DEBRICK-LINUX.md`.

## Memory: the main constraint

- Core A's resident code must leave at least **3072 B free** (`AP_MIN_FREE`, lowered from 4096 on 2026-11-25,
  the user's choice, for layouts phase 2); otherwise the link fails. The 8 GB `PROGRAMS=1` build has
  **3428 B free** (2026-11-27), 356 B above the floor (RAM: 576 B free); without Programs 72 B more
  (`build/cfw/ap_size.txt` shows the numbers). `LTO=0` no longer
  fits at all (964 B over even with `AP_MIN_FREE=0`).
- New features go into **overlays**: swap-in code windows, opened through gates in `firmware/startup/ap_ovl.c`
  (`GATE`, `npx_ok()`). Files wholly in an overlay are placed by object name in `firmware/firmware_ap.ld`;
  mostly resident files tag their overlay functions `OVL_TEXT(<overlay>)` (`services/ovl.h`), so they can
  still use LTO. `NOCROSSREFS` stops resident code calling into an overlay. Room left:
  - modes window (14 KB): viz about 3.1 KB free, npx about 2.1 KB, lay (layouts, and `config_load`) about 3.7 KB, usbdac 6.7 KB,
    usbout 4.9 KB, dev 1 KB.
  - library phase window (12 KB): lwalk and lbuild by turns, layb (the built-in layouts' text, 7.2 KB),
    and prg (the program loader with its API: 6.3 KB, its USB serial and sound state in the overlay).
  - fonts window: about 3.6 KB, used as the CJK glyph cache.
  - lib window: about 630 B.
- LTO: the link compiles each function apart (`-flto-partition=max`) so a string shared by resident and
  overlay code stays resident, on every core (`-flto=auto`: a clean core-A build about 13 s in WSL's own
  filesystem, about 53 s on `/mnt/c`). `ap_startup.o`, `watchdog.o` and `bootguard.o` stay out of LTO (inline
  asm; the `.noinit` crash record at `0x0302FFC8`), and so do the files the linker script places by name (keep
  new overlay code on `OVL_TEXT` tags or section names, not `*file.o` patterns, or that file loses LTO).
- What fills it (2026-11-25, 9be3351): about 13 KB of strings, 7.6 KB of them log messages; the biggest
  functions are opening the next track (8.3 KB with the format readers inlined), the UI loop (7.8 KB) and the
  audio poll. Measured and not worth it: `-Oz` (same code as `-Os` here), no inlining or cloning (0 or worse).
  Done: LTO for `ap_ovl.c` (+288 B), `firmware/libc/udiv64.c` in place of libgcc's 754-byte 64-bit division
  (+596 B). Keep: the 8x8 font (`drivers/font8x8.c`: the recovery screens draw with it before the fonts load),
  the update prompt (recovery) and the clock's self-test (it runs while the clock switches). Still possible:
  shorter log messages (1 to 3 KB, less detail in `ECHOLOG.TXT`), boot-only code in an overlay (riskier).
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
| Layouts (docs/LAYOUTS.md): Now Playing (`[np]`), list rows (`[list]`), the header (`[header]`), questions, sliders, info pages, Lyrics, Up Next, Spectrum, the EQ editor, the name entry (`[question]`, `[slider]`, `[info]`, `[lyrics]`, `[next]`, `[spectrum]`, `[eq]`, `[rename]`) from `GILB/LAYOUTS/*.txt`, Settings › Display › Layout per Look | `ui/layout.c` (lay overlay: parser, Now Playing's drawing, also `config_load`), `ui/layout_text.c` (the built-in Modern and Retro layouts: the layb overlay, `ap_lay_builtin`), `ui/layout.h`; parsed into `g_lay`, `g_lrow`, `g_lhdr` (`ui/screens.c`) by `ap_lay_load()`; rows drawn by `draw_row` in `ui/nav.c`, the header by `hdr_pass` in `ui/frame.c` (both resident); phase 3's screens parsed one at a time as shown (`lay_screen`, into `g_lscr`) and drawn by `lay_screen_draw` in `ui/nav.c` (resident); the views' and the EQ's sections parsed before their overlay loads (`ap_npx_draw`, `ap_eq` in `startup/ap_ovl.c`); Spectrum's and the EQ's bars drawn by `ui/spectrum.c`, `ui/eq_page.c`; the USB DAC screen stays hand-drawn (`dac_draw`); `GILB.CFG` `layout=`, `retro_layout=`; Echo Library `src/layouts.js` (a test checks its built-ins against `ui/layout_text.c`) and Themes › Layouts in `src/main.js` (every screen above) |
| Simulator screenshots | `sim/README.md`: `--headless --script FILE` presses keys and saves BMPs |
| Library views (Albums grid, Years, ...) | `firmware/ui/nav.c`; index build in `lib_build.c` (index version 3) |
| Self-test (Settings › System) | `firmware/services/selftest.c` (dev overlay) |
| Screens and settings menus | `firmware/ui/screens.c` |
| Bluetooth screen and its volume (`bt_fixed=`) | `firmware/startup/ap_bt.c`; the module's driver `firmware/drivers/bt_jieli.c`; `docs/BT_DEVICE_NOTES.md` |
| Old code, tools and build targets (not built) | `archive/` (`archive/README.md`, `archive/Makefile.legacy`) |
| Feature list and device-test status | `docs/ROADMAP.md` |
| Hardware decoding results | `docs/HIFI_PLAN.md` §4.5 |
| Programs (beta): Settings › System › Programs runs `GILB/APPS/*.APP` | `firmware/services/programs.c` (the prg overlay: loader and API table), `prg_cdc.c` (USB serial), `prg_sound.c` (sound); `sdk/` (`include/gilb_app.h`, the API's source of truth; `API.md`, the reference; `README.md`; `examples/hello`, `memtest`); `docs/PROGRAMS.md` (how it works, version by version) |
| Doom (a program) | `ports/doom/`: the MG24 port's game in `source/`, `include/` (GILB changes marked `GILB:`), the board layer in `gilb/`, `doom.ld` (four memory blocks), `tools/pack.py` (DOOM.APP and DOOM.BIN), `tools/music/` (the song renderer); `ports/doom/README.md` (keys, memory, sound, music); `docs/DOOM_PLAN.md` (history and measurements) |
| PC keyboard for programs | `tools/gilb_keys.ps1` (Windows): one byte a key change over the player's USB serial port |
| Download site | `site/` on Railway (https://gilb-production.up.railway.app): the newest release, Early Beta (pre-releases tagged with "early"), 512 MB board (beta) (other pre-releases); `GITHUB_TOKEN` is set in Railway; `site/README.md` |
| Printed case | `case/` (OpenSCAD, 74a5917); the design lives in a private repo |

## State of the to-do list

Everything is committed and pushed. The last change is **f63a626** (Doom music, 2026-11-27); the last
firmware change is **e02b042** (Programs API 5); Echo Library's is **738be7b**.

Releases on GitHub (AlanRSV/GILB): **v2026.11.25** (Latest, 8 GB); pre-releases
**v2026.11.26-512m-beta** (the 512 MB board, still in testing, from 27fa81a) and **v2026.11.26-early**
(early beta with Programs and Doom, 8 GB, from 3673af0: Doom zipped as `DOOM-GILB.zip`, since GitHub
refuses `.APP` files; no WAD and no songs, which can't be shared; it predates Doom's music).
**v2026.11.27-early** (2026-11-27, from 370d028): the same with Doom's music (the notes say how to render
the songs; still no WAD or songs attached). The site lists both early betas, newest first.

Done and confirmed on the device (2026-11-26 and 27, newest first):

- **Doom music** (f63a626): each song rendered once on the PC with the port's own OPL2 synthesis
  (`ports/doom/tools/music`, `doom_music DOOM1.WAD OUTDIR`: 13 songs, 38 MB) and streamed from
  `GILB/APPS/DOOMMUS/D_*.PCM` (live OPL2 has no room on the player)
- **Programs API 5, Doom's sound effects** (e02b042): `sound_open` plays a ring the program fills
  through the audio DMA. The CS43131 plays nothing below 32 kHz (Doom upsamples 11025 Hz to 44100), and
  `usb_dwc2_init`'s `hw_on` cuts the DAC's supply (so `serial_open` brings the DAC back up)
- **Programs API 4, USB control channel** (b0dea78): a USB serial port for programs (CDC, ID 071B:3209)
  and `tools/gilb_keys.ps1`: Doom plays from a PC keyboard. Four fixes in the shared CDC driver (it never
  armed bulk OUT, among others). Programs became beta-only (`PROGRAMS=1`)
- **Doom** (25ef915, the MG24 port of Nicola Wrachien): E1M1 plays at 8 to 34 fps; core A at 96 MHz and
  1.1 ms storage requests are the limits. API 3 (`more_mem_a`) lends the fonts window and PCM ring. Also
  fixed then: a POWER tap left any program (`a_poll`'s `now | 1`)
- **Programs** (05ef36a, d16aa07): the loader, API 1 and 2 (`more_mem`: core B's RAM, where code runs
  4.8x slower); `sdk/API.md` (fe28546) is the reference
- Download site: Beta and Early Beta sections (eca5792, e871faa, 3673af0), working while GitHub refuses
  the server; the user set `GITHUB_TOKEN` in Railway (logs say "with a token")
- Theme kept after a restart; MENU on Now Playing opens the album (27fa81a, issue 2)
- Printed case (74a5917)

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
- Layouts phase 3a (8194390; Echo Library 08911af): questions, sliders and info pages from layout files
- Layouts phase 3b (891844f; Echo Library 738be7b): Lyrics, Up Next, Spectrum, the EQ editor and the name
  entry from layout files; the built-ins moved to the layb overlay. A too-wide pixel-font line switches to
  its alt font only with `wide=alt` (Retro's questions); without it the line is cut, as before
- Core A size (9be3351): LTO for the overlay gates and a compact 64-bit division: 3448 -> 4332 B free;
  LTO builds in parallel
- Bluetooth › Volume (f121933): Player, Fixed -6 dB or Fixed 0 dB (`bt_fixed=` in `GILB.CFG`). The JieLi
  module re-digitizes the DAC's analog output and sends SBC (`docs/BT_DEVICE_NOTES.md`), so a turned-down
  DAC raised its noise; fixed holds the DAC at that level while connected (volume keys change nothing).
  No better codec is possible without replacing the module (its firmware is its own)
- Repo tidy (ddebeaf): 884 files nothing used moved to `archive/` (git mv); the Makefile from 618 to 259
  lines; today's firmware builds the same code byte for byte (the overlays' ABI hash and CRC aside).
  `docs/` sorted the same way later (32 files left; the rest in `archive/docs/`, `archive/README.md`)
- Download site `site/` (9da590d, 173aabe): one page and a dependency-free Node server showing the newest
  GitHub release's images (`HIFIEC90.IMG`, `MINIV900.IMG`) and extra files (`CJK.BIN`), for Railway (Root
  Directory `site`, `site/README.md`). Release images are built without `FONT_IMG` (Build, above)
- Second Echo Mini (8 GB, stock until then, never had GILB): the release image (173aabe, no stock fonts,
  SHA-256 DF0348749DCE5EDC54C39241B42109D6F52825A2788B17526DE69738287A9640) installed through the stock
  updater and everything worked (2026-11-25): a new user's first install is proven
- The update image's own outer header (941a0a0): `tools/make_fw1.py` `write_outer_header` writes it (brand
  "GILB", the commit as build string, version 0, a part chain with no parts); RECHO's image is no longer an
  input (`--ref`/`REF_IMG` removed) and none of its bytes are in GILB's images. Same bytes as before outside
  the header. Tested 2026-11-25: the second player put back on stock 4.0.0 (through GILB's updater), then
  the new image through the STOCK updater: all works. Field comparison: `docs/fw1-packing.md` §11
- Retro's own themes (0b51e63; Echo Library 3d1156c): Settings › Display › Theme lists the current Look's
  themes, each Look remembers its own, and Echo Library's Themes page edits both
- the self-test's audio step: 4 s a side, 2 s gap; its test feed now writes one 32-bit word a channel
  (b741491). The DAC chips' channel values stay as stock's: b741491's swap was reverted in e2cab7d (the
  earphones in that test were in backwards). `LR TEST/LR test.flac` (`build/lrtest/`) checks left/right.

The README has new Modern (Pink) and Retro screenshots and the corrected credits (RECHO's creator first).

Dropped or set aside:

- **Skipped or no change needed:** the Bluetooth reconnect edge case (never seen), Composers view, faster
  copying, crash-safe settings (already safe), key timing step 2, screen tearing.
- **Hardware decoding:** closed. The FFT output can't be read back. The FLAC block stays on.

Still open:

- **Doom:** faster core A while a program runs (the FLAC clock point, A at 224 MHz, behind
  `services/cpuclk.c`'s self-test), fewer storage requests, E1M6 may not fit the level cache (Doom stops
  with a message), save games untested. - **Programs:** the player's own menus from the PC keyboard (only programs get it); the 512 MB board with
  Programs never built or tested.

- **Layouts:** done but for the USB DAC screen, which stays hand-drawn (its overlay holds the window, and
  it needs covers and badges the resident drawer lacks). Resident code has 920 B above the floor:
  anything sizeable must still go into an overlay or replace resident code.

- **First public release** (the user's current goal). Done: no stock fonts in the image (173aabe), our own
  header (941a0a0), a clean first install through the stock updater (second player). Prepared
  2026-11-25 (b0c21ea):
  1. Boards: **8 GB only** (`HIFIEC90.IMG`); the README and notes say 512 MB is untested and not released.
  2. Licenses: `THIRD_PARTY.md` (every decoder and font, its license and license file);
     `release/OFL.txt` (Fusion Pixel's header + SIL OFL 1.1, shipped beside `CJK.BIN`). Fusion Pixel's
     header is from its repo's `LICENSE-OFL` (no Reserved Font Name).
  3. README: 8 GB releases, a Download pointer (GitHub Releases, `site/`), the QEMU suite described as
     ReChord's and unpublished (GILB tests with host tests, the simulator and the players), CJK.BIN,
     Retro's themes, layouts, known limits, `THIRD_PARTY.md`.
  4. `release/build.sh` (run in WSL from the repo root: `sh release/build.sh`) does a clean 8 GB build
     with `REVERSE_LISTS=1` and no `FONT_IMG`, and puts `HIFIEC90.IMG`, `CJK.BIN` (from `build/CJK.BIN`),
     `OFL.txt` and `SHA256SUMS.txt` in `build/release/`. The image's header carries the commit's short hash, so
     **rebuild after the release commit** and put that commit and the SHA-256s into `release/NOTES.md`
     (the draft notes: `COMMIT`, `SHA_IMG`, `SHA_CJK` placeholders). A trial build on 45a8a0f plus these
     uncommitted docs: 3992 B free; CJK.BIN's SHA-256 94AF030E060984F7582B92C83755B86653BD5E3B4EB378267D610455C83105E6
     (it doesn't change between builds).
  5. **Done 2026-11-25.** Committed b0c21ea, rebuilt (`HIFIEC90.IMG` SHA-256
     C8B53D6E8C3FBAE934182206962AAE3120643E7B812468B0A3812039E5DF17DF), flashed ("All works"), published
     as GitHub release **v2026.11.25** (tag on b0c21ea; `HIFIEC90.IMG`, `CJK.BIN`, `OFL.txt`; GitHub's
     digests match), and the user deployed `site/` on Railway: https://gilb-production.up.railway.app
     (checked: shows the release, its SHA-256 and the extra files). Release notes must not be
     hard-wrapped: GitHub keeps each line break (`release/NOTES.md` has one line per paragraph).
  - Next releases: commit, `sh release/build.sh` in WSL, fill `release/NOTES.md`'s placeholders into
    `build/release/NOTES.md`, the user flashes it, then a new release with `gh release create <tag> --target <commit>
    --title ... --notes-file build/release/NOTES.md build/release/{HIFIEC90.IMG,CJK.BIN,OFL.txt}` (the GitHub
    CLI is installed and signed in since 2026-11-25; in Git Bash it may need
    `PATH="$PATH:/c/Program Files/GitHub CLI"`). Publishing needs the user's yes.

- **Second Echo Mini:** on GILB (941a0a0's image) since 2026-11-25; optional: run its self-test, copy
  `GILB/CJK.BIN` to it.

- **docs/:** sorted 2026-11-25: 32 files kept (those the README, this file or the code cite); 435 moved to
  `archive/docs/` (the stock decompile, RE plans, SDK-era status), listed in `archive/README.md`.

The list is now driven by the user's own ideas (their to-do memory has the history). Ask what they want next.
