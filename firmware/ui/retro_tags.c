/*
 * retro_tags.c -- the playing song's year, for Look: Retro's Now Playing (ui/retro.c). The player's
 * tag reader keeps no year, and the library's (TAGS_EXT) is too big for this overlay: a small
 * search instead, over the file's first 128 KiB, where the tags sit (FLAC and Ogg comments, ID3v2,
 * an MP4's moov when it comes first):
 *   DATE= / YEAR=   Vorbis comments (FLAC, Ogg Vorbis, Opus)
 *   TDRC / TYER     ID3v2.4 / 2.3 frames (MP3, and FLAC or WAV with an ID3 tag), ISO-8859-1,
 *                   UTF-8 or UTF-16
 *   \xA9day         MP4 (M4A): its data atom's text
 * The first four digits after the one found first, 1000..2999; 0 when none. Once a track.
 */
#include <stdint.h>
#include <string.h>

#include "audio/audio.h"
#include "services/fs.h"
#include "ui/np_extra.h"

#define SCAN_MAX (128u * 1024u)
#define CHUNK 1024u
#define OVER 24u                          /* (a key and its value across two reads) */

unsigned retro_year(void);

static unsigned year_at(const uint8_t *p, const uint8_t *end)
{
    unsigned y = 0, n = 0;

    for (; p < end && n < 4; p++) {
        if (*p == 0 || *p == 0xFF || *p == 0xFE)
            continue;                     /* (UTF-16's zero bytes and BOM) */
        if (*p < '0' || *p > '9')
            return 0;
        y = y * 10u + (unsigned)(*p - '0');
        n++;
    }
    return n == 4 && y >= 1000u && y < 3000u ? y : 0u;
}

static int ieq5(const uint8_t *p, const char *k)  /* 5 bytes, ASCII case-insensitive */
{
    for (int i = 0; i < 5; i++)
        if ((p[i] | 0x20u) != (uint8_t)(k[i] | 0x20))
            return 0;
    return 1;
}

static unsigned find(const uint8_t *b, unsigned n)
{
    const uint8_t *end = b + n;

    for (unsigned i = 0; i + OVER <= n; i++) {
        const uint8_t *p = b + i;
        unsigned y = 0;

        if ((p[0] | 0x20u) == 'd' && ieq5(p, "date="))
            y = year_at(p + 5, end);
        else if ((p[0] | 0x20u) == 'y' && ieq5(p, "year="))
            y = year_at(p + 5, end);
        else if (p[0] == 'T' && (!memcmp(p, "TDRC", 4) || !memcmp(p, "TYER", 4)))
            y = year_at(p + 11, end);     /* id, size, flags (10), the encoding byte */
        else if (p[0] == 0xA9 && !memcmp(p + 1, "day", 3))
            y = year_at(p + 20, end);     /* (its data atom: size, "data", type, locale) */
        if (y)
            return y;
    }
    return 0;
}

unsigned retro_year(void)
{
    static uint32_t seq;
    static unsigned year;
    uint8_t buf[CHUNK + OVER];            /* (on the stack: an overlay's .bss stays in RAM) */
    audio_status_t st;
    fs_file_t *f;
    uint32_t off = 0;
    long got;

    if (seq == g_np.seq)
        return year;
    seq = g_np.seq;
    year = 0;
    audio_status(&st);
    if (st.seq != g_np.seq || !(f = fs_open_ref(&st.track.ref)))
        return 0;
    while (!year && off < SCAN_MAX && fs_seek(f, (long)off) >= 0 && (got = fs_read(f, buf, sizeof buf)) > (long)OVER) {
        year = find(buf, (unsigned)got);
        off += CHUNK;
    }
    fs_close(f);
    return year;
}
