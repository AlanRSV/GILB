# Doom for GILB

Doom as a GILB program (`docs/PROGRAMS.md`), run from **Settings › System › Programs**, with its
sound effects and music. It needs GILB with program API version 3 (`more_mem_a`); version 4 adds
a PC keyboard and version 5 the sound. Programs are a beta: only GILB images built with `PROGRAMS=1`
have them (`docs/PROGRAMS.md`).

## Sound

`gilb/gilb_sound.c`: the MG24 port's mixer, 8 channels in stereo, the effects read from the WAD. It
mixes at the effects' 11025 Hz and writes each sample four times, interpolated, at 44100 Hz: the
player's DAC plays nothing below 32 kHz. Its ring is 2048 frames (46 ms, 16 KiB of AHI), topped up
every 10 ms at most from the key reads, the renderer's NetUpdate calls, the end of each storage
read and the panel's bands. It keeps an absolute frame count, so effects carry on in step after a
long frame, and it saves the WAD reader's position, since it runs inside other reads (it once sent
the level set-up's reads astray).

## Music

The port synthesised its music live (OPL2, `source/opl2.c`: 57 KB of code and tables, and a good
share of the CPU). The player has neither to spare, so each song is rendered once on the PC with
that same synthesis and MUS player, and Doom streams the result:

```sh
make -C ports/doom/tools/music
mkdir -p mus && ports/doom/tools/music/doom_music DOOM1.WAD mus
```

That writes one `D_xxxx.PCM` per song (16-bit mono at 11025 Hz, one pass of the score; 38 MB for
the shareware WAD's 13). Copy them to `GILB/APPS/DOOMMUS` on the player. `gilb/gilb_sound.c` mixes
the current song in as it does an effect, at the music volume, reading 4 KiB at a time; level music
loops, the title tune doesn't. Without the files the music is silent. Like the WAD, the songs are
made from your own `DOOM1.WAD` and not shared.

On the way: the synthesis read past its attack table when an envelope level drifted above 511
(`D_INTER`); `opl2.c` now holds it in range.

## Credits and license

- **The game** (`source/`, `include/`) is [MG24 Doom BLE](https://github.com/next-hack/MG24_Doom_BLE)
  by **Nicola Wrachien** (next-hack), commit `fc02a55`. That port is based on **doomhack**'s
  [GBA Doom](https://github.com/doomhack/GBADoom), with **Kippykip**'s additions, and on PrBoom
  (id Software, Lee Killough, Colin Phipps, Florian Schulze and the others in each file's header).
  GPLv2 or later; every file keeps its header. The GILB changes are marked `GILB:`.
- **`gilb/printf.c`, `gilb/printf.h`**: Marco Paland's [printf](https://github.com/mpaland/printf), MIT.
- **`gilb/doom_ble.h`**: from the same port (its Bluetooth types; there's no Bluetooth game here).
- **`gilb/`, the rest**: the board layer for GILB, GPLv2 or later like the game.
- **The WAD converter** is the port's `MCUDoomWadUtil` (GPLv2), built as it is.

GILB is GPL-3.0, which can include GPLv2-or-later code.

## Build

In WSL:

```sh
make -C ports/doom
```

That writes `build/DOOM.APP` and `build/DOOM.BIN`. The WAD is converted once, with the port's tool:

```sh
cd ~/ext/mg24doom/MCUDoomWadUtil
gcc -O2 -std=gnu99 -w -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration \
    -Wno-error=int-conversion -o mcudoomwadutil *.c
./mcudoomwadutil DOOM1.WAD DOOM.WAD        # mcudoom_0_4.wad must be in the current folder
```

Copy `DOOM.APP`, `DOOM.BIN` and the converted `DOOM.WAD` to `GILB/APPS` on the player. The
shareware `DOOM1.WAD` (v1.9, 4,196,020 B, MD5 `f0cefca49926d00903cf57551d901abe`) is freely
distributable; Ubuntu's `doom-wad-shareware` package has it.

## The screen

The game renders natively at **320 × 170**: the 3D view is 320 × 138 with square pixels and the
full 32-row status bar under it. The port already scales its 320 × 200 pictures (menus, title,
intermission) to the screen's height, so those are squeezed to 85 % vertically. Only one frame
is kept (no screen melt between screens).

## Keys

The Echo Mini's keys share one resistor ladder, so GILB sees only one of VOL+, VOL-, PLAY, BACK
and MENU at a time (POWER is separate). Walking is therefore a toggle, and turning and firing work
while walking:

| | In the game | In the menus |
|---|---|---|
| VOL- (held) | turn left | up |
| VOL+ (held) | turn right | down |
| PLAY | walk on / off; held: walk backwards | select |
| MENU (held) | fire | right (sliders) |
| BACK | use (doors, switches); held: the menu | back |
| POWER | next weapon | left |
| VOL+ and VOL- together | the map | |
| POWER held 2 s | back to GILB | back to GILB |

On the title, intermission and finale screens, PLAY or MENU goes on and BACK opens the menu.

### A PC keyboard

With GILB's API version 4 and a cable in (answer the cable question with "charge only"), start Doom,
then on the PC run `powershell -ExecutionPolicy Bypass -File tools\gilb_keys.ps1` and keep its window
in front. These keys can be held together:

| Key | | Key | |
|---|---|---|---|
| W / up arrow | forward | S / down arrow | backwards |
| left / right arrow | turn | A / D | strafe |
| Ctrl | fire | Space | use |
| Shift | run | Q / E | previous / next weapon |
| Tab | the map | Esc / Enter | the menu / select |

The player's own keys keep working alongside. On an older GILB, Doom runs without the keyboard.

## Memory

`doom.ld` places the game in four blocks:

| Block | Size | What |
|---|---|---|
| APP 0x03030000 | 112 KiB | `DOOM.APP`: the header, the first stage, the hot code (renderer, game logic) |
| AHI 0x03077900 | 161.75 KiB | core A, lent by `more_mem_a`: the math tables, the cold code (menus, level set-up, intermission, finale), the 8-bit frame, the RGB565 bands, the WAD's read cache |
| BLO 0x01000000 | 128 KiB | core B, lent by `more_mem`: `.data`, `.bss`, `info.c`'s tables, then the zone (the game's heap) |
| BHI 0x01021000 | 252 KiB | core B: the game's "internal flash", for the level data and textures |

- **The first stage.** GILB loads only `DOOM.APP` (112 KiB at most). Its first stage
  (`gilb/gilb_main.c`) asks for the memory, reads `DOOM.BIN` (the AHI and BLO blocks), checks it was
  built with this `DOOM.APP` and its CRC, then starts the game.
- **The game's 16-bit pointers** (`include/i_memory.h`) reach 256 KiB from `RAM_PTR_BASE`, so every
  variable and the zone live in core B's RAM from 0x01000000.
- **The WAD** stays a file (`gilb/extMemory.h`, `gilb/gilb_wad.c`), read through 2 KiB pages kept in RAM.
- **The level cache.** On the MG24 the level data and textures went to about 1 MB of internal
  flash. Here they go to BHI's 252 KiB. Textures that don't fit are read from the WAD each time.
  A level whose own data doesn't fit stops with "too big for the player's memory". Each level start
  logs what it used (`doom: E1M1: cache ...` in `ECHOLOG.TXT`).
- **Speed.** Code in core B's RAM runs 4.8× slower, so none is placed there. Data there reads 1.6×
  slower (the zone, the level data).
