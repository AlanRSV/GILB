/*
 * lyrics.c -- see lyrics.h. Reads only what it needs: the FLAC metadata block headers, the
 * Vorbis comment keys, the ID3v2 frame headers; the lyrics themselves are read straight into
 * the caller's buffer (never through a window), then converted to UTF-8 in place.
 */
#include <string.h>

#include "services/lyrics.h"

static uint32_t be24(const uint8_t *p) { return (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | be24(p + 1); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t syncsafe(const uint8_t *p) { return (uint32_t)(p[0] & 0x7F) << 21 | (uint32_t)(p[1] & 0x7F) << 14 | (uint32_t)(p[2] & 0x7F) << 7 | (p[3] & 0x7F); }

typedef struct {
    tags_read_at_t rd;
    void *u;
    uint32_t size;
    uint8_t *out;
    uint32_t cap;
} ctx_t;

/* n bytes at off into b: 1 when all of them are there. */
static int rd(const ctx_t *c, uint32_t off, void *b, uint32_t n)
{
    return off <= c->size && n <= c->size - off && c->rd(c->u, off, b, n) == n;
}

/* ASCII key k[0..n) == s, ignoring case */
static int key_is(const uint8_t *k, uint32_t n, const char *s)
{
    uint32_t i = 0;

    while (i < n && s[i] && (k[i] | 0x20) == (uint8_t)(s[i] | 0x20))
        i++;
    return i == n && !s[i];
}

enum { E_LATIN1, E_UTF16, E_UTF16BE, E_UTF8 };

/* The text at out[in .. in+n) in encoding enc, as UTF-8 from out[0] on (it never overtakes what
 * is still to be read: the caller put the text far enough back). Returns its length. */
static uint32_t to_utf8(const ctx_t *c, uint32_t in, uint32_t n, int enc)
{
    uint8_t *b = c->out;
    const uint8_t *p = b + in;
    uint32_t o = 0;
    int le = 0;

    if (enc == E_UTF8) {
        if (n >= 3u && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
            p += 3, n -= 3;
        while (o < n && p[o])
            o++;
        memmove(b, p, o);
        b[o] = 0;
        return o;
    }
    if (enc == E_UTF16 && n >= 2u && ((p[0] == 0xFF && p[1] == 0xFE) || (p[0] == 0xFE && p[1] == 0xFF))) {
        le = p[0] == 0xFF;
        p += 2, n -= 2;
    }
    while (n) {
        uint32_t cp, k = 1, w;

        if (enc == E_LATIN1) {
            cp = p[0];
        } else {
            if (n < 2u)
                break;
            cp = le ? (uint32_t)p[1] << 8 | p[0] : (uint32_t)p[0] << 8 | p[1];
            k = 2;
            if (cp >= 0xD800u && cp < 0xDC00u && n >= 4u) {
                uint32_t lo = le ? (uint32_t)p[3] << 8 | p[2] : (uint32_t)p[2] << 8 | p[3];

                if (lo >= 0xDC00u && lo < 0xE000u) {
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    k = 4;
                }
            }
            if (cp >= 0xD800u && cp < 0xE000u)
                cp = 0xFFFDu;
        }
        if (!cp)
            break;
        w = cp < 0x80u ? 1u : cp < 0x800u ? 2u : cp < 0x10000u ? 3u : 4u;
        if (o + w >= c->cap || b + o + w > p + k)
            break;
        if (w == 1u) {
            b[o++] = (uint8_t)cp;
        } else {
            b[o++] = (uint8_t)((0xF0u << (4u - w)) | cp >> (6u * (w - 1u)));
            for (uint32_t i = w - 1u; i--; )
                b[o++] = (uint8_t)(0x80u | (cp >> (6u * i) & 0x3Fu));
        }
        p += k;
        n -= k;
    }
    b[o] = 0;
    return o;
}

/* A Vorbis comment block [p, end): the value of LYRICS= (or UNSYNCEDLYRICS=). */
static uint32_t vorbis(const ctx_t *c, uint32_t p, uint32_t end)
{
    uint8_t b[16];
    uint32_t cnt;

    if (end - p < 8u || !rd(c, p, b, 4) || le32(b) > end - p - 8u)
        return 0;
    p += 4u + le32(b);
    if (!rd(c, p, b, 4))
        return 0;
    cnt = le32(b);
    p += 4u;
    while (cnt-- && end - p >= 4u && rd(c, p, b, 4)) {
        uint32_t n = le32(b), k = 0, m;

        p += 4u;
        if (n > end - p)
            return 0;
        m = n < sizeof b ? n : (uint32_t)sizeof b;
        if (m && rd(c, p, b, m)) {
            while (k < m && b[k] != '=')
                k++;
            if (k < m && (key_is(b, k, "LYRICS") || key_is(b, k, "UNSYNCEDLYRICS"))) {
                uint32_t v = n - k - 1u;

                if (v > c->cap - 1u)
                    v = c->cap - 1u;
                if (!rd(c, p + k + 1u, c->out, v))
                    return 0;
                return to_utf8(c, 0, v, E_UTF8);
            }
        }
        p += n;
    }
    return 0;
}

/* FLAC: an ID3v2 tag in front is stepped over, then the metadata blocks up to VORBIS_COMMENT. */
static uint32_t flac(const ctx_t *c)
{
    uint8_t b[10];
    uint32_t pos = 0;

    if (rd(c, 0, b, 10) && !memcmp(b, "ID3", 3))
        pos = 10u + syncsafe(b + 6) + (b[5] & 0x10u ? 10u : 0u);
    if (!rd(c, pos, b, 4) || memcmp(b, "fLaC", 4))
        return 0;
    for (pos += 4u; rd(c, pos, b, 4); ) {
        uint32_t len = be24(b + 1), type = b[0] & 0x7Fu;

        pos += 4u;
        if (len > c->size - pos)
            return 0;
        if (type == 4u)
            return vorbis(c, pos, pos + len);
        if ((b[0] & 0x80u) || type == 127u)
            return 0;
        pos += len;
    }
    return 0;
}

/* A TXXX description d[0..n) (any ID3 encoding: its ASCII letters) that names lyrics. */
static int lyrics_desc(const uint8_t *d, uint32_t n)
{
    uint8_t a[16];
    uint32_t k = 0;

    for (uint32_t i = 0; i < n && k < sizeof a; i++)
        if (d[i] >= 0x20 && d[i] < 0x7F)
            a[k++] = d[i];
    return key_is(a, k, "LYRICS") || key_is(a, k, "UNSYNCEDLYRICS") || key_is(a, k, "USLT");
}

/* The ID3v2 tag at pos, within [pos, lim): its USLT (v2.3/2.4) or ULT (v2.2) frame's text, else a
 * TXXX (TXX) frame described LYRICS, UNSYNCEDLYRICS or USLT (what some taggers, ffmpeg among
 * them, write). */
static uint32_t id3v2(const ctx_t *c, uint32_t pos, uint32_t lim)
{
    uint8_t h[10];
    uint32_t ver, fl, len, p, end, hl;

    if (lim - pos < 10u || !rd(c, pos, h, 10) || memcmp(h, "ID3", 3) || h[3] < 2 || h[3] > 4 ||
        ((h[6] | h[7] | h[8] | h[9]) & 0x80))
        return 0;
    ver = h[3];
    fl = h[5];
    len = syncsafe(h + 6);
    p = pos + 10u;
    end = len < lim - p ? p + len : lim;
    if ((ver < 4u && (fl & 0x80u)) || (ver == 2u && (fl & 0x40u)))
        return 0;                      /* whole-tag unsync (v2.2/2.3), v2.2 compression: not read */
    if (fl & 0x40u) {                  /* extended header */
        if (!rd(c, p, h, 4))
            return 0;
        len = ver == 3u ? be32(h) + 4u : syncsafe(h);
        p = len < 4u || len > end - p ? end : p + len;
    }
    hl = ver == 2u ? 6u : 10u;
    while (end - p >= hl && rd(c, p, h, hl)) {
        uint32_t fsz = ver == 2u ? be24(h + 3) : ver == 3u ? be32(h + 4) : syncsafe(h + 4);
        uint32_t ffl = ver == 2u ? 0u : (uint32_t)h[8] << 8 | h[9], skip = 0, raw, n, at, i, lang = 3;
        int un = 0, enc;

        if (!h[0])
            return 0;                  /* padding */
        p += hl;
        if (fsz > end - p)
            return 0;
        if (ver == 2u ? !memcmp(h, "TXX", 3) : !memcmp(h, "TXXX", 4))
            lang = 0;                  /* (a TXXX has no language) */
        else if (!(ver == 2u ? !memcmp(h, "ULT", 3) : !memcmp(h, "USLT", 4))) {
            p += fsz;
            continue;
        }
        if (ver == 3u) {
            if (ffl & 0x00C0u)         /* compressed, encrypted */
                return 0;
            skip = ffl & 0x0020u ? 1u : 0u;
        } else if (ver == 4u) {
            if (ffl & 0x000Cu)
                return 0;
            skip = (ffl & 0x0040u ? 1u : 0u) + (ffl & 0x0001u ? 4u : 0u);
            un = (ffl & 0x0002u) || (fl & 0x80u);
        }
        if (fsz < skip + lang + 2u || !rd(c, p + skip, h, 1) || h[0] > 3) {
            p += fsz;
            continue;
        }
        enc = h[0];
        /* the rest (description, text) at the back of the buffer, as much as fits: UTF-8 from
         * the front then never passes it (Latin-1 doubles at most, UTF-16 x1.5) */
        n = fsz - skip - 1u - lang;
        raw = enc == E_UTF8 ? c->cap - 1u : enc == E_LATIN1 ? (c->cap - 1u) / 2u : (c->cap - 1u) / 3u * 2u;
        if (n > raw)
            n = raw;
        at = c->cap - n;
        if (!rd(c, p + skip + 1u + lang, c->out + at, n))
            return 0;
        if (un) {                      /* FF 00 -> FF, in place (only shrinks) */
            uint32_t o = 0;

            for (i = 0; i < n; i++) {
                c->out[at + o++] = c->out[at + i];
                if (c->out[at + i] == 0xFF && i + 1u < n && !c->out[at + i + 1u])
                    i++;
            }
            n = o;
        }
        /* step over the content description: NUL-terminated (UTF-16: two NULs, aligned) */
        i = 0;
        if (enc == E_LATIN1 || enc == E_UTF8) {
            while (i < n && c->out[at + i])
                i++;
            i++;
        } else {
            while (i + 1u < n && (c->out[at + i] || c->out[at + i + 1u]))
                i += 2u;
            i += 2u;
        }
        if (i >= n || (!lang && !lyrics_desc(c->out + at, i))) {
            p += fsz;                  /* (another TXXX: the buffer is only scratch until found) */
            continue;
        }
        return to_utf8(c, at + i, n - i, enc);
    }
    return 0;
}

/* WAV: the "id3 " chunk's tag. */
static uint32_t wav(const ctx_t *c)
{
    uint8_t b[12];
    uint32_t end, p = 12;

    if (!rd(c, 0, b, 12) || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4))
        return 0;
    end = le32(b + 4) >= 4u && le32(b + 4) <= c->size - 8u ? le32(b + 4) + 8u : c->size;
    while (end - p >= 8u && rd(c, p, b, 8)) {
        uint32_t n = le32(b + 4);

        p += 8u;
        if (n > end - p)
            n = end - p;
        if (!memcmp(b, "id3 ", 4) || !memcmp(b, "ID3 ", 4))
            return id3v2(c, p, p + n);
        if (n + (n & 1u) >= end - p)
            break;
        p += n + (n & 1u);
    }
    return 0;
}

uint32_t lyrics_tag(tags_read_at_t rdf, void *u, uint32_t size, int kind, char *out, uint32_t cap)
{
    ctx_t c = { rdf, u, size, (uint8_t *)out, cap };
    uint32_t n = 0;

    if (cap < 16u)
        return 0;
    out[0] = 0;
    if (kind == TAGS_FLAC)
        n = flac(&c);
    else if (kind == TAGS_WAV)
        n = wav(&c);
    else if (kind == TAGS_MP3)
        n = id3v2(&c, 0, size);
    out[n] = 0;
    return n;
}

/* ---- parsing ------------------------------------------------------------------------------- */

#define NO_TIME 0xFFFFu

static int digits(const char *s, uint32_t *i, uint32_t e, uint32_t max, uint32_t *v)
{
    uint32_t n = 0;

    *v = 0;
    while (*i < e && n < max && s[*i] >= '0' && s[*i] <= '9')
        *v = *v * 10u + (uint32_t)(s[(*i)++] - '0'), n++;
    return (int)n;
}

/* "[mm:ss]", "[mm:ss.x..]" or "[mm:ss:xx]" at s[i] (i at '['): its time in tenths and the
 * index past ']', or -1. */
static long stamp(const char *s, uint32_t i, uint32_t e, uint32_t *ds)
{
    uint32_t m, sec, f = 0;

    i++;
    if (!digits(s, &i, e, 3, &m) || i >= e || s[i++] != ':' || digits(s, &i, e, 2, &sec) != 2 || sec > 59u)
        return -1;
    if (i < e && (s[i] == '.' || s[i] == ':')) {
        uint32_t j = ++i, k;

        if (!digits(s, &i, e, 3, &k))
            return -1;
        f = (uint32_t)(s[j] - '0');
    }
    if (i >= e || s[i] != ']')
        return -1;
    m = m * 600u + sec * 10u + f;
    *ds = m < NO_TIME ? m : NO_TIME - 1u;
    return (long)i + 1;
}

static void add(lyrics_t *l, uint32_t at, uint32_t ds)
{
    if (l->n < LYR_LINES) {
        l->at[l->n] = (uint16_t)at;
        l->ds[l->n++] = (uint16_t)ds;
    }
}

void lyrics_parse(lyrics_t *l, uint32_t len)
{
    char *t = l->text;
    uint32_t r = 0, w = 0, any = 0;
    long off = 0;                      /* [offset:] in ms: positive shows the lines sooner */

    l->n = 0;
    l->synced = 0;
    if (len > LYR_TEXT - 1u)
        len = LYR_TEXT - 1u;
    t[len] = 0;
    while (r < len) {
        uint32_t e = r, s = r, times[8], nt = 0, start, ds;
        long q;

        while (e < len && t[e] != '\n' && t[e])
            e++;
        while (s < e && t[s] == '[') {
            if ((q = stamp(t, s, e, &ds)) >= 0) {
                if (nt < 8u)
                    times[nt++] = ds;
                s = (uint32_t)q;
                continue;
            }
            /* a header ("[ar:...]", "[offset:+250]") is a line of its own: dropped */
            if (key_is((const uint8_t *)t + s + 1u, 7, "offset:")) {
                uint32_t i = s + 8u, v;
                int neg = i < e && t[i] == '-';

                if (i < e && (t[i] == '-' || t[i] == '+'))
                    i++;
                if (digits(t, &i, e, 6, &v))
                    off = neg ? -(long)v : (long)v;
            }
            s = e;
            nt = 0;
            break;
        }
        if (s < e || nt) {
            uint32_t i;

            while (s < e && (t[s] == ' ' || t[s] == '\t'))
                s++;
            start = w;
            for (i = s; i < e; i++) {
                uint32_t j = i, d;

                /* enhanced LRC's word times "<mm:ss.xx>": dropped */
                if (t[i] == '<' && (j = i + 1u, digits(t, &j, e, 3, &d)) && j < e && t[j] == ':') {
                    while (j < e && t[j] != '>')
                        j++;
                    if (j < e) {
                        i = j;
                        continue;
                    }
                }
                t[w++] = t[i] == '\t' ? ' ' : t[i];
            }
            while (w > start && (t[w - 1] == ' ' || t[w - 1] == '\r'))
                w--;
            t[w++] = 0;
            if (nt) {
                any = 1;
                for (i = 0; i < nt; i++)
                    add(l, start, times[i]);
            } else if (w - start > 1u) {
                add(l, start, NO_TIME);
            }
        }
        r = e + 1u;
    }
    if (any) {                         /* timed: untimed lines go, the rest in time order */
        uint32_t n = 0;

        for (uint32_t i = 0; i < l->n; i++) {
            long ds = l->ds[i];

            if (ds == NO_TIME)
                continue;
            ds -= off / 100;
            ds = ds < 0 ? 0 : ds >= NO_TIME ? NO_TIME - 1 : ds;
            l->at[n] = l->at[i];
            l->ds[n++] = (uint16_t)ds;
        }
        l->n = (uint16_t)n;
        for (uint32_t i = 1; i < n; i++) {
            uint16_t a = l->at[i], d = l->ds[i];
            uint32_t j = i;

            for (; j && l->ds[j - 1] > d; j--) {
                l->at[j] = l->at[j - 1];
                l->ds[j] = l->ds[j - 1];
            }
            l->at[j] = a;
            l->ds[j] = d;
        }
        l->synced = 1;
    }
}

int lyrics_line(const lyrics_t *l, uint32_t ms)
{
    uint32_t ds = ms / 100u;
    int lo = 0, hi = (int)l->n - 1, k = -1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;

        if (l->ds[mid] <= ds) {
            k = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return k;
}
