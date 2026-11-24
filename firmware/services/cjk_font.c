/*
 * cjk_font.c -- glyphs the built-in fonts lack, read one at a time from GILB/CJK.BIN.
 *
 * Japanese, Chinese and Korean (and Greek, Cyrillic) titles: tens of thousands of glyphs, far more
 * than core A's RAM holds, so they stay in a file on the internal storage and drivers/text.c asks
 * for the ones a screen shows (text_ext_set: it keeps them in a cache, so a screen reads each once).
 * tools/gilb_cjkfont.py makes the file from Fusion Pixel 12 px (SIL OFL 1.1), Japanese style.
 *
 * CJK.BIN (little-endian):
 *   0   "GCJK1\0\0\0"
 *   8   u32 ranges (at most CJK_RANGES), u32 record size (32)
 *   16  per range: u32 first, u32 last (code points), u32 offset of first's record
 *   ..  per code point of each range, a 32-byte record: text_xglyph_t as it is (adv 0: none)
 * No file: every such code point draws as the font's box, as before. Looked for again after a
 * while (CJK_RETRY_MS): copied on over USB, it is found without a restart.
 */
#include <stdint.h>
#include <string.h>

#include "fs.h"
#include "drivers/clock.h"
#include "drivers/text.h"
#include "services/cjk_font.h"

#define CJK_PATH     "/Internal/GILB/CJK.BIN"
#define CJK_RANGES   24
#define CJK_RETRY_MS 10000u

static struct { uint32_t first, last, off; } s_rng[CJK_RANGES];
static uint8_t s_nr;
static int8_t s_state;                 /* 0: not read yet, 1: ready, -1: missing or bad */
static uint32_t s_since;               /* (-1: when) */
static long s_size;                    /* the file's size when its ranges were read */

static int load(fs_file_t *f)
{
    uint8_t h[16];
    uint32_t n;

    if (fs_read(f, h, sizeof h) != (long)sizeof h || memcmp(h, "GCJK1\0\0\0", 8))
        return -1;
    memcpy(&n, h + 8, 4);
    if (n == 0 || n > CJK_RANGES || h[12] != sizeof(text_xglyph_t))
        return -1;
    if (fs_read(f, s_rng, 12u * n) != (long)(12u * n))
        return -1;
    s_nr = (uint8_t)n;
    s_size = fs_file_size(f);
    return 0;
}

static int get(uint32_t c, text_xglyph_t *out)
{
    fs_file_t *f;
    long at = -1;
    int ok = -1;

    if (s_state < 0 && time_ms() - s_since < CJK_RETRY_MS)
        return -1;
    if (s_state > 0) {                   /* no file opened for a code point it has no range for */
        unsigned i;

        for (i = 0; i < s_nr && !(c >= s_rng[i].first && c <= s_rng[i].last); i++)
            ;
        if (i == s_nr)
            return -1;
    }
    f = fs_open_read(CJK_PATH);
    if (!f) {
        s_state = -1;
        s_since = time_ms();
        return -1;
    }
    if (s_state <= 0 || fs_file_size(f) != s_size) {   /* first time, or a new file */
        s_state = 1;
        if (load(f)) {
            s_state = -1;
            s_since = time_ms();
            fs_close(f);
            return -1;
        }
    }
    for (unsigned i = 0; i < s_nr; i++)
        if (c >= s_rng[i].first && c <= s_rng[i].last)
            at = (long)(s_rng[i].off + (c - s_rng[i].first) * sizeof *out);
    if (at >= 0 && fs_seek(f, at) == at && fs_read(f, out, sizeof *out) == (long)sizeof *out && out->adv)
        ok = 0;
    fs_close(f);
    return ok;
}

void cjk_font_init(void *cache, uint32_t len)
{
    text_ext_set(get, cache, len);
}
