#!/bin/sh
# Build a GILB release's files (run in WSL from the repo root, on the commit being released, with a
# clean working tree: the image's header carries that commit).
#   sh release/build.sh                 8 GB board: build/release/HIFIEC90.IMG
#   BOARD=512m sh release/build.sh      512 MB board: build/release-512m/MINIV900.IMG
# Both with REVERSE_LISTS=1 and no FONT_IMG, plus CJK.BIN, OFL.txt and SHA256SUMS.txt.
set -e
BOARD=${BOARD:-8g}
if [ "$BOARD" = 8g ]; then
    IMG=HIFIEC90.IMG; OUT=build/release
    STOCK_IMG=${STOCK_IMG:-'stock/4.0.0/ECHO MINI V4.0.0/HIFIEC40.IMG'}
else
    IMG=MINIV900.IMG; OUT=build/release-512m
    STOCK_IMG=${STOCK_IMG:-'stock/4.0.0/ECHO MINI V4.0.0/MINIV400.IMG'}
fi
CJK=${CJK:-build/CJK.BIN}

# (No dirty check here: WSL's git sees every file as changed in a Windows checkout. Check
# `git status` on Windows first.)
rm -rf build/cfw build/coreb "$OUT"
make cfw-ap BOARD=$BOARD REVERSE_LISTS=1
make cfw-img BOARD=$BOARD REVERSE_LISTS=1 STOCK_IMG="$STOCK_IMG"
mkdir -p "$OUT"
cp build/$IMG "$OUT/"
cp "$CJK" "$OUT/CJK.BIN"
cp release/OFL.txt "$OUT/OFL.txt"
(cd "$OUT" && sha256sum $IMG CJK.BIN OFL.txt > SHA256SUMS.txt && cat SHA256SUMS.txt)
echo "commit: $(git rev-parse --short=7 HEAD)"
grep -i free build/cfw/ap_size.txt || true
