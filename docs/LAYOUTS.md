# Layouts: screens described by files

A layout says where each part of a screen goes, how big it is, its font, colour and alignment.
The player draws the screen from it; Echo Library's layout editor makes them (drag, resize, add).
Colours are the theme's: a layout names colour roles (`text`, `accent`, ...), never colours, so one
layout works with every theme.

Started 2026-11-24 (the user's idea, after the EchoPod theme maker). Three phases; each is a build
tested on the player.

| Phase | Screens | State |
|---|---|---|
| 1 | Now Playing (its cover view) | done 2026-11-25, tested on the device |
| 2 | the header, and the list rows (every list: Home, Library, Folders, Settings, options, pickers) | built 2026-11-25, on test |
| 3 | questions, sliders, info pages, Lyrics, Up Next, Spectrum, the EQ editor, Rename, the USB DAC screen | planned |

## Where layouts come from

- Each Look (Settings › Display › Look) has a **Layout** setting: `Built-in`, or a file from
  `GILB/LAYOUTS/<name>.txt` on the internal storage. Modern and Retro remember theirs apart
  (`layout=` and `retro_layout=` in `GILB.CFG`).
- `Built-in` is the Look's own layout, written in this same format (`ui/layout.c`): the screens look as
  they did before layouts.
- A file names only the screens it changes (`[np]`, `[list]`, `[header]`); the others stay the Look's
  built-in.
- Settings › Display › Refresh themes reads the layout folder again too.

## The file

Plain text, one element a line, `#` starts a comment. Sections name the screen. Numbers are pixels on
the 320 x 170 panel (x to the right, y down). The player reads a file's first 4095 bytes and up to
`LAY_MAX` elements a screen; a line it does not understand is skipped (a typo costs that line only).

```
# Retro's Now Playing, as built in
[np]
box   3 22 314 145 color=panel
text  8 37 160 field=queue font=px
cover 8 42 84 tint=1 noart=solid
text  102 58 210 field=title font=px
text  102 80 210 field=artist font=px
...
```

Each line: the kind, then its place, then `key=value` options.

| Kind | Place | Options |
|---|---|---|
| `text x y w` | `y` is the **baseline** | `field=`, `font=`, `align=l\|c\|r`, `color=`, `wrap=1\|2` (lines), `up=` / `grow=` (when it wraps: its first line goes up `up` px and every later `flow=1` element down `grow`), `flow=1` |
| `cover x y size` | a square | `tint=1` (in the theme's two colours, as Retro), `noart=hue\|solid` (no picture: the album's colour block, or a block of `color=`), `pause=1` (the paused mark on its corner) |
| `box x y w h` | | `color=` (filled) |
| `frame x y w h` | | `color=` (1 px outline) |
| `bar x y w h` | | `kind=progress\|volume`, `style=line\|outline` (line: a track and its fill with a head; outline: a frame with the fill inside), `color=` (fill), `track=` |
| `badge x y` | `x=-1`: after the badge before it | `field=codec\|format`, `style=fill\|outline`, `hires=1` (the hi-res colour when it is) |

Text fields: `title`, `artist`, `album`, `year`, `queue` ("3 of 12"), `elapsed`, `remaining` ("-2:31"),
`total`, `kbps` ("99 kbps vbr"), `facts` ("1411 KBPS · 41.6 MB"), `next` ("NEXT  SONG"), `codec`,
`format` ("24/96"), or `text="..."` for words of your own.

Fonts: `px` (GILB's pixel font; a line it lacks uses `t17` from 18 px up, else `b13`), `s8`, `b13`,
`m13`, `t17`, `d28`.

Colour roles: the theme's `text`, `text_dim`, `text_off`, `accent`, `on_accent`, `background`,
`surface`, `divider`, `sel_bg`, `sel_text`, `bar_fill`, `bar_track`, `badge_bg`, `badge_text`, `hires`,
`danger`, `success`, `warn`, and `panel` (Retro's Now Playing panel: 14 % from the background to the
text).

## Now Playing (phase 1)

- The layout draws the whole body, y 20..169 (the header above it is `[header]`'s).
- The band at y 138 and below is shared: while the volume or the seek band shows (a VOL press, a held
  skip), they replace the layout's elements there, as before.
- Each second the player redraws only the layout's `elapsed`, `remaining` and `total` texts and its
  progress bars; a pause redraws its covers' pause marks; a new track, all of it.
- Lyrics, Up Next and Spectrum (BACK) are not layouts yet (phase 3).

## List rows (phase 2): `[list]`

Every list (Home, Library, Folders, Settings, options, pickers) draws each row from these elements.
Places are from the row's top-left; `row N` sets the row's height (18..42 px; the list shows as many
rows as fit in its 147 px from y 21, up to 8).

```
# Modern's rows, as built in
[list]
row 21
select 0 0 316 21 color=sel_bg
mark 6 5 numx=2 numw=18 numy=15 numfont=s8
value 24 15 286 font=b13 color=value gap=8
label 24 15 286 font=b13 bold=m13
chev 304 5 take=10
scroll 316
```

| Kind | Place | What it is | Options |
|---|---|---|---|
| `select x y w h` | | the chosen row's highlight | `color=` |
| `mark x y` | its icon's top-left | the row's icon, the playing speaker, a radio or check mark, or a track's number | `numx=` / `numw=` / `numy=` / `numfont=` (the number's box and baseline), `icons=0` (no section icons), `color=`, `on=` (a mark that is on) |
| `label x y w` | baseline | the row's name | `font=`, `bold=` (the font of a bold row), `after=` (its indent after a mark), `color=` |
| `value x y w` | baseline | the words on the right (a count, an artist, a time, a setting) | `font=`, `gap=` (room kept before it), `max=` (its widest), `color=` |
| `chev x y` | | the arrow of a row that opens something | `take=` (room it takes from the row's right), `style=text` (a ">" in `font=`), `color=` |
| `scroll x` | | the scroll bar at x | `style=line\|outline` |
| `box`, `frame` | | as in `[np]`, on every row | |

The colour role `value` is the theme's colour for a row's value. The chosen row's words use `sel_text`.

## The header (phase 2): `[header]`

The header (y 0..19) has parts packed from the right edge in their lines' order, then the title in
what is left. A part's line has no place, only its gap and its look.

```
# Modern's header, as built in
[header]
part battery
part percent font=s8 y=13 gap=3
part clock font=s8 y=13 gap=8
part volume font=b13 y=14 gap=8 color=text
part output gap=13
part lock gap=15 color=warn
part play font=b13 y=14 gap=8
title font=m13 y=14
```

Parts: `battery` (its icon), `percent`, `clock`, `volume`, `output` (the speaker, Bluetooth or USB
icon), `lock`, `play` (the play mark and the elapsed time, outside Now Playing). Leave a line out to
hide that part.

| Option | |
|---|---|
| `gap=` | the room before it (to its right) |
| `font=`, `y=` (baseline), `color=` | its words |
| `alt=` | the font for letters `px` lacks |
| `on=` (title) | its icon's colour |
| `np=volume` (title) | Now Playing's title is the play mark and the volume ("-12dB"), as Retro |

## On the player

- The parser, Now Playing's drawing and the built-in layouts are their own overlay, `lay`
  (`startup/ap_ovl.c`, `firmware_ap.ld`), which also reads `GILB.CFG`. It parses once the Look or its
  layout changes. When it cannot be had (USB-C earphones hold the window), Now Playing draws Modern's
  hand-made screen, and lists and the header keep the layout parsed last.
- The parsed layout (`lay_el_t`, 16 bytes an element: `g_lay`, `g_lrow`, `g_lhdr`) and the redraw
  rectangles live in main memory, so the second's redraw never loads the overlay.
- Lists and the header are drawn by main memory's code (`ui/nav.c`, `ui/frame.c`) from the parsed
  elements; that replaced Retro's hand-written lists. To fit, the main-memory reserve went from 4 KiB
  to 3 KiB (2026-11-25, the user's choice).
