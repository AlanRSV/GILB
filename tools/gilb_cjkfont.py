#!/usr/bin/env python3
"""GILB/CJK.BIN: the glyphs the player's fonts lack (Japanese, Chinese, Korean, Greek, Cyrillic),
read one at a time by firmware/services/cjk_font.c.

  gilb_cjkfont.py build FONT.bdf OUT.bin    OUT from a 12 px BDF font
  gilb_cjkfont.py show FONT.bdf TEXT        TEXT's glyphs as '#' rows (a quick look)

The font: Fusion Pixel 12 px, proportional, Japanese style (fusion-pixel-12px-proportional-ja.bdf,
https://github.com/TakWolf/fusion-pixel-font, SIL OFL 1.1): its release's BDF zip. Copy OUT to the
player as GILB/CJK.BIN (internal storage).

The file (little-endian; services/cjk_font.c reads it):
  "GCJK1\\0\\0\\0", u32 ranges, u32 record size (32),
  per range: u32 first, u32 last, u32 offset of first's record,
  per code point of each range: adv, w, h, left, top (s8, from the baseline, y down), 3 x 0,
  12 rows of u16 (bit 15 = the left column). adv 0: the font has no glyph for it.
"""
import struct, sys

# Wanted blocks; each is trimmed to the glyphs the font has (a block it lacks entirely is left out).
BLOCKS = [
    (0x0370, 0x03FF),   # Greek
    (0x0400, 0x04FF),   # Cyrillic
    (0x1100, 0x11FF),   # Hangul Jamo
    (0x2E80, 0x2FDF),   # CJK radicals, Kangxi radicals
    (0x3000, 0x30FF),   # CJK punctuation, Hiragana, Katakana
    (0x3100, 0x31FF),   # Bopomofo, Hangul compatibility Jamo, Katakana extension
    (0x3200, 0x33FF),   # enclosed CJK, CJK compatibility
    (0x3400, 0x4DBF),   # CJK extension A
    (0x4E00, 0x9FFF),   # CJK unified ideographs
    (0xAC00, 0xD7AF),   # Hangul syllables
    (0xF900, 0xFAFF),   # CJK compatibility ideographs
    (0xFE30, 0xFE4F),   # CJK compatibility forms
    (0xFF00, 0xFFEF),   # half-width and full-width forms
]
REC = 32
MAX_W, MAX_H = 16, 12


def read_bdf(path):
    """{code: (adv, w, h, xoff, yoff, [row ints, w bits each, MSB left])}"""
    out, cur = {}, None
    with open(path, encoding='utf-8') as f:
        for line in f:
            k, _, v = line.strip().partition(' ')
            if k == 'STARTCHAR':
                cur = {'rows': None}
            elif cur is None:
                continue
            elif k == 'ENCODING':
                cur['code'] = int(v.split()[0])
            elif k == 'DWIDTH':
                cur['adv'] = int(v.split()[0])
            elif k == 'BBX':
                cur['bbx'] = [int(x) for x in v.split()]
            elif k == 'BITMAP':
                cur['rows'] = []
            elif k == 'ENDCHAR':
                w, h, xo, yo = cur['bbx']
                rows = [int(r, 16) >> (len(r) * 4 - w) if w else 0 for r in cur['rows']]
                if cur.get('code', -1) >= 0:
                    out[cur['code']] = (cur['adv'], w, h, xo, yo, rows)
                cur = None
            elif cur['rows'] is not None:
                cur['rows'].append(k)
    return out


def record(g):
    adv, w, h, xo, yo, rows = g
    top = -(yo + h)
    while h and rows and rows[0] == 0:                 # trim blank rows (the ink box)
        rows, h, top = rows[1:], h - 1, top + 1
    while h and rows and rows[-1] == 0:
        rows, h = rows[:-1], h - 1
    if w > MAX_W or h > MAX_H:
        return None
    if not h:
        w = 0
    words = [(r << (16 - w)) & 0xFFFF if w else 0 for r in rows] + [0] * (12 - h)
    return struct.pack('<BBBbb3x12H', adv, w, h, xo, top, *words)


def build(bdf, dst):
    font = read_bdf(bdf)
    ranges, body, skipped, count = [], b'', 0, 0
    for a, b in BLOCKS:
        have = [c for c in range(a, b + 1) if c in font]
        if not have:
            continue
        ranges.append((have[0], have[-1]))
    head = 16 + 12 * len(ranges)
    table = b''
    for first, last in ranges:
        table += struct.pack('<III', first, last, head + len(body))
        for c in range(first, last + 1):
            r = record(font[c]) if c in font else None
            if c in font and r is None:
                skipped += 1
            if r is None:
                r = b'\0' * REC
            else:
                count += 1
            body += r
    out = b'GCJK1\0\0\0' + struct.pack('<II', len(ranges), REC) + table + body
    open(dst, 'wb').write(out)
    print('%s: %d B, %d glyphs in %d ranges%s' % (dst, len(out), count, len(ranges),
          ', %d too big left out' % skipped if skipped else ''))
    for first, last in ranges:
        print('  U+%04X..U+%04X' % (first, last))


def show(bdf, text):
    font = read_bdf(bdf)
    for ch in text:
        g = font.get(ord(ch))
        if not g:
            print(ch, 'missing')
            continue
        r = record(g)
        adv, w, h, left, top = struct.unpack('<BBBbb', r[:5])
        rows = struct.unpack('<12H', r[8:])
        print('%s adv %d box %dx%d left %d top %d' % (ch, adv, w, h, left, top))
        for y in range(h):
            print('  ' + ''.join('#' if rows[y] & 0x8000 >> x else '.' for x in range(w)))


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == 'build':
        build(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 4 and sys.argv[1] == 'show':
        show(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)
