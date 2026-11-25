# Third-party code and fonts in GILB

GILB is licensed under **GPL-3.0** (`LICENSE`). Its update image (`HIFIEC90.IMG`) contains the code
below, each under its own license, all compatible with GPL-3.0. The full license texts are in the
directories listed. The source of every release is this repository at the commit the release names.

## Decoders (core B, `firmware/coreb/`)

| Code | Used for | License | License file |
|---|---|---|---|
| libmad | MP3 | GPL-2.0-or-later | `libmad/COPYING` |
| FAAD2 2.11.4 (libfaad) | AAC, HE-AAC | GPL-2.0-or-later | `faad/COPYING` |
| libdemac (Rockbox) | APE (Monkey's Audio) | GPL-2.0-or-later | `demac/COPYING` |
| libwma, libwmapro (Rockbox, ports of FFmpeg) | WMA v1/v2, WMA Pro | LGPL-2.1-or-later (FFmpeg's files), GPL-2.0-or-later (Rockbox's) | file headers, `wma/README.rechord` |
| FFmpeg n7.1 `wmalosslessdec.c` | WMA Lossless | LGPL-2.1-or-later | file header, `wma/README.rechord` |
| Tremor, libogg (Xiph.Org) | Ogg Vorbis | BSD-3-Clause | `tremor/COPYING`, `ogg/COPYING` |
| libopus 1.5.2, opusfile 0.12 (Xiph.Org and others) | Opus | BSD-3-Clause | `opus/COPYING`, `opus/opusfile/COPYING` |
| WavPack (David Bryant) | WavPack | BSD-3-Clause | `wavpack/COPYING` |
| Apple ALAC | ALAC | Apache-2.0 | `alac/LICENSE` |
| dr_flac (David Reid) | FLAC | public domain or MIT-0 | end of `firmware/player/codecs/dr_flac/dr_flac.h` |

Each directory's `README.rechord` says where its code came from and what was changed.
(`alac/APPLE_LICENSE.txt` is the older Apple Public Source License that came with Apple's repository;
the files GILB uses are under Apache-2.0.)

## Fonts

| Font | Where | License | License file |
|---|---|---|---|
| Noto Sans, Noto Sans Devanagari, Noto Sans Math (The Noto Project Authors) | built into the image (`firmware/drivers/fonts/`) | SIL OFL 1.1 | `firmware/drivers/fonts/OFL.txt` |
| Fusion Pixel Font 12 px (TakWolf; with glyphs from Ark Pixel Font, Cubic 11, Galmuri) | `GILB/CJK.BIN`, a separate optional file | SIL OFL 1.1 | `release/OFL.txt`, shipped beside `CJK.BIN` |
| PX, GILB's pixel font | built into the image (`tools/gilb_pixfont.py`) | GPL-3.0 (GILB's own) | `LICENSE` |

## Not included

No stock firmware (SNOWSKY / FiiO) and no part of RECHO is in this repository or in GILB's images.
Building the image needs a stock image as an input (`STOCK_IMG`), but none of its bytes end up in the
update file. See the README's Credits for who did what.
