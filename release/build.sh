#!/bin/sh
# Build a GILB release's files into build/release/ (run in WSL from the repo root, on the commit
# being released, with a clean working tree: the image's header carries that commit).
#   sh release/build.sh
# Out: HIFIEC90.IMG (8 GB board, REVERSE_LISTS=1, no FONT_IMG), CJK.BIN and OFL.txt, SHA256SUMS.txt.
set -e
STOCK_IMG=${STOCK_IMG:-'stock/4.0.0/ECHO MINI V4.0.0/HIFIEC40.IMG'}
CJK=${CJK:-build/CJK.BIN}
OUT=build/release

# (No dirty check here: WSL's git sees every file as changed in a Windows checkout. Check
# `git status` on Windows first.)
rm -rf build/cfw build/coreb "$OUT"
make cfw-ap BOARD=8g REVERSE_LISTS=1
make cfw-img BOARD=8g REVERSE_LISTS=1 STOCK_IMG="$STOCK_IMG"
mkdir -p "$OUT"
cp build/HIFIEC90.IMG "$OUT/"
cp "$CJK" "$OUT/CJK.BIN"
cp release/OFL.txt "$OUT/OFL.txt"
(cd "$OUT" && sha256sum HIFIEC90.IMG CJK.BIN OFL.txt > SHA256SUMS.txt && cat SHA256SUMS.txt)
echo "commit: $(git rev-parse --short HEAD)"
grep -i free build/cfw/ap_size.txt || true
