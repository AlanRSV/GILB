/*
 * test_lyrics.c -- services/lyrics.c: lyrics from FLAC, MP3 (ID3v2.3 UTF-16, ID3v2.4 UTF-8) and
 * WAV tags built here byte by byte, and LRC / plain parsing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "services/lyrics.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

typedef struct { const uint8_t *b; uint32_t n; unsigned reads; } file_t;

static uint32_t rd(void *u, uint32_t off, void *buf, uint32_t n)
{
    file_t *f = u;

    f->reads++;
    if (off > f->n)
        return 0;
    if (n > f->n - off)
        n = f->n - off;
    memcpy(buf, f->b + off, n);
    return n;
}

static uint8_t g[65536];
static uint32_t gn;
static void put(const void *p, uint32_t n) { memcpy(g + gn, p, n); gn += n; }
static void put8(uint32_t v) { g[gn++] = (uint8_t)v; }
static void le32(uint32_t v) { for (int i = 0; i < 4; i++) put8(v >> (8 * i)); }
static void be32(uint32_t v) { for (int i = 3; i >= 0; i--) put8(v >> (8 * i)); }
static void ss32(uint32_t v) { put8(v >> 21 & 0x7F); put8(v >> 14 & 0x7F); put8(v >> 7 & 0x7F); put8(v & 0x7F); }

static const char *k_lrc = "[ar:Clairo]\n[ti:Alewife]\n[00:01.50]First line\r\n[00:05.00]Second <00:05.50>line\n"
                           "[00:03.00][00:09.25]Chorus\n[00:12.00]\nno time here\n";

static void flac_file(const char *val)
{
    uint32_t vlen = (uint32_t)strlen(val) + 7u, blen;

    gn = 0;
    put("fLaC", 4);
    put8(0x00); put8(0); put8(0); put8(34);              /* STREAMINFO */
    memset(g + gn, 0, 34); gn += 34;
    put8(0x01); put8(0); put8(0); put8(100);             /* PADDING */
    memset(g + gn, 0, 100); gn += 100;
    blen = 4 + 6 + 4 + 4 + 12 + 4 + vlen;
    put8(0x84); put8(blen >> 16); put8(blen >> 8); put8(blen);   /* VORBIS_COMMENT, last */
    le32(6); put("vendor", 6);
    le32(2);
    le32(12); put("TITLE=Song A", 12);
    le32(vlen); put("lyrics=", 7); put(val, vlen - 7u);
    memset(g + gn, 0xAB, 5000); gn += 5000;               /* audio */
}

static void id3_uslt(int ver, int enc, const uint8_t *text, uint32_t tn)
{
    uint32_t fs = 1 + 3 + (enc == 1 ? 4 : 2) + tn, start;

    put("ID3", 3); put8(ver); put8(0); put8(0); ss32(10 + 10 + fs + 50);
    put("TIT2", 4); if (ver == 4) ss32(6); else be32(6); put8(0); put8(0); put8(3); put("Hello", 5);
    put("USLT", 4); if (ver == 4) ss32(fs); else be32(fs); put8(0); put8(0);
    start = gn;
    put8(enc); put("eng", 3);
    if (enc == 1) { put8(0xFF); put8(0xFE); put8(0); put8(0); } else { put8('d'); put8(0); }
    put(text, tn);
    if (gn - start != fs) { printf("fixture size\n"); exit(1); }
    memset(g + gn, 0, 50); gn += 50;                       /* padding */
}

static void test_flac(void)
{
    static char out[LYR_TEXT];
    file_t f;

    flac_file("Plain one\nPlain two");
    f = (file_t){ g, gn, 0 };
    CHECK(lyrics_tag(rd, &f, gn, TAGS_FLAC, out, sizeof out) == 19);
    CHECK(!strcmp(out, "Plain one\nPlain two"));
    CHECK(f.reads < 20);
    /* none: other kind, no LYRICS key */
    CHECK(lyrics_tag(rd, &f, gn, TAGS_MP3, out, sizeof out) == 0 && !out[0]);
    /* longer than the buffer: cut */
    {
        static char big[9000];

        memset(big, 'x', sizeof big - 1);
        flac_file(big);
        f = (file_t){ g, gn, 0 };
        CHECK(lyrics_tag(rd, &f, gn, TAGS_FLAC, out, 100) == 99 && strlen(out) == 99);
    }
}

static void test_mp3(void)
{
    static char out[LYR_TEXT];
    uint8_t u16[64];
    const char *s = "Héllo\nwörld";
    uint32_t n = 0;
    file_t f;

    /* v2.4, UTF-8 */
    gn = 0;
    id3_uslt(4, 3, (const uint8_t *)"[00:01.00]Hi\n[00:02.00]There", 28);
    memset(g + gn, 0xFF, 3000); gn += 3000;
    f = (file_t){ g, gn, 0 };
    CHECK(lyrics_tag(rd, &f, gn, TAGS_MP3, out, sizeof out) == 28);
    CHECK(!strcmp(out, "[00:01.00]Hi\n[00:02.00]There"));
    /* v2.3, UTF-16 LE with BOM */
    u16[n++] = 0xFF; u16[n++] = 0xFE;
    for (const char *p = "Hello\nworld"; *p; p++) { u16[n++] = (uint8_t)*p; u16[n++] = 0; }
    u16[n++] = 0xE9; u16[n++] = 0x00;                      /* é */
    gn = 0;
    id3_uslt(3, 1, u16, n);
    f = (file_t){ g, gn, 0 };
    CHECK(lyrics_tag(rd, &f, gn, TAGS_MP3, out, sizeof out) == 13);
    CHECK(!strcmp(out, "Hello\nworld\xC3\xA9"));
    /* v2.3, Latin-1 */
    gn = 0;
    id3_uslt(3, 0, (const uint8_t *)"H\xE9llo\nw\xF6rld", 11);
    f = (file_t){ g, gn, 0 };
    CHECK(lyrics_tag(rd, &f, gn, TAGS_MP3, out, sizeof out) == strlen(s));
    CHECK(!strcmp(out, s));
    /* WAV with an "id3 " chunk */
    {
        uint32_t tag0, tagn;

        gn = 0;
        put("RIFF", 4); le32(0); put("WAVE", 4);
        put("fmt ", 4); le32(16); memset(g + gn, 1, 16); gn += 16;
        put("data", 4); le32(1000); memset(g + gn, 0, 1000); gn += 1000;
        put("id3 ", 4); le32(0);
        tag0 = gn;
        id3_uslt(4, 3, (const uint8_t *)"Wav words", 9);
        tagn = gn - tag0;
        g[tag0 - 4] = (uint8_t)tagn; g[tag0 - 3] = (uint8_t)(tagn >> 8);
        g[4] = (uint8_t)(gn - 8); g[5] = (uint8_t)((gn - 8) >> 8);
        f = (file_t){ g, gn, 0 };
        CHECK(lyrics_tag(rd, &f, gn, TAGS_WAV, out, sizeof out) == 9);
        CHECK(!strcmp(out, "Wav words"));
    }
    /* TXXX frames: another one first, then lyrics as ffmpeg writes them (TXXX "USLT") */
    {
        static const char gain[] = "\x00REPLAYGAIN_TRACK_GAIN\x00-6.0 dB";
        static const char ly[] = "\x00USLT\x00Line one\nLine two";
        uint32_t g1 = sizeof gain - 1u, g2 = sizeof ly - 1u;

        gn = 0;
        put("ID3", 3); put8(3); put8(0); put8(0); ss32(20 + g1 + g2);
        put("TXXX", 4); be32(g1); put8(0); put8(0); put(gain, g1);
        put("TXXX", 4); be32(g2); put8(0); put8(0); put(ly, g2);
        f = (file_t){ g, gn, 0 };
        CHECK(lyrics_tag(rd, &f, gn, TAGS_MP3, out, sizeof out) == 17);
        CHECK(!strcmp(out, "Line one\nLine two"));
        /* only the other TXXX: none */
        gn = 0;
        put("ID3", 3); put8(3); put8(0); put8(0); ss32(10 + g1);
        put("TXXX", 4); be32(g1); put8(0); put8(0); put(gain, g1);
        f = (file_t){ g, gn, 0 };
        CHECK(lyrics_tag(rd, &f, gn, TAGS_MP3, out, sizeof out) == 0);
    }
    /* truncated tag: nothing, no crash */
    f = (file_t){ g, 30, 0 };
    CHECK(lyrics_tag(rd, &f, 30, TAGS_MP3, out, sizeof out) == 0);
}

static void test_parse(void)
{
    static lyrics_t l;
    uint32_t n = (uint32_t)strlen(k_lrc);

    memcpy(l.text, k_lrc, n);
    lyrics_parse(&l, n);
    CHECK(l.synced == 1);
    CHECK(l.n == 5);
    CHECK(l.ds[0] == 15 && !strcmp(l.text + l.at[0], "First line"));
    CHECK(l.ds[1] == 30 && !strcmp(l.text + l.at[1], "Chorus"));
    CHECK(l.ds[2] == 50 && !strcmp(l.text + l.at[2], "Second line"));
    CHECK(l.ds[3] == 92 && !strcmp(l.text + l.at[3], "Chorus"));
    CHECK(l.ds[4] == 120 && !strcmp(l.text + l.at[4], ""));      /* an instrumental gap */
    CHECK(lyrics_line(&l, 0) == -1);
    CHECK(lyrics_line(&l, 1500) == 0);
    CHECK(lyrics_line(&l, 4999) == 1);
    CHECK(lyrics_line(&l, 5000) == 2);
    CHECK(lyrics_line(&l, 600000) == 4);

    /* plain: lines in order, empty ones and section labels dropped */
    {
        const char *p = "Verse one\r\n\r\n[Chorus]\nLa la\n   \nEnd";

        n = (uint32_t)strlen(p);
        memcpy(l.text, p, n);
        lyrics_parse(&l, n);
        CHECK(l.synced == 0 && l.n == 3);
        CHECK(!strcmp(l.text + l.at[0], "Verse one") && !strcmp(l.text + l.at[1], "La la") &&
              !strcmp(l.text + l.at[2], "End"));
    }
    /* offset: +500 ms shows every line half a second sooner */
    {
        const char *p = "[offset:+500]\n[00:10.00]A\n[00:00.20]B";

        n = (uint32_t)strlen(p);
        memcpy(l.text, p, n);
        lyrics_parse(&l, n);
        CHECK(l.synced && l.n == 2 && l.ds[0] == 0 && l.ds[1] == 95);
    }
    /* full buffer of lines: capped, no overflow */
    {
        uint32_t k = 0;

        while (k + 12u < LYR_TEXT - 1u)
            k += (uint32_t)sprintf(l.text + k, "[%02u:%02u.00]x\n", (k / 12u) / 60u % 100u, (k / 12u) % 60u);
        lyrics_parse(&l, k);
        CHECK(l.n == LYR_LINES);
    }
    /* empty */
    lyrics_parse(&l, 0);
    CHECK(l.n == 0 && lyrics_line(&l, 1000) == -1);
}

/* Damaged files and text: no read outside the buffers (run under ASan/UBSan), lines inside text. */
static void test_fuzz(void)
{
    static lyrics_t l;
    static uint8_t base[3][4096];
    static uint32_t bn[3];
    static const char *const marks = "[]:.<>0123456789\n\r ofst+-";
    uint32_t seed = 12345;

    flac_file("[00:01.00]a\nb");
    memcpy(base[0], g, bn[0] = gn < 4096 ? gn : 4096);
    gn = 0;
    id3_uslt(4, 1, (const uint8_t *)"\xFF\xFE" "a\0b\0", 6);
    memcpy(base[1], g, bn[1] = gn);
    gn = 0;
    id3_uslt(3, 0, (const uint8_t *)"abc", 3);
    memcpy(base[2], g, bn[2] = gn);
    for (int it = 0; it < 20000; it++) {
        int k = it % 3;
        uint32_t n = bn[k];
        file_t f;

        memcpy(g, base[k], n);
        for (int m = 0; m < 4; m++) {
            seed = seed * 1103515245u + 12345u;
            g[(seed >> 8) % n] = (uint8_t)(seed >> 20);
        }
        seed = seed * 1103515245u + 12345u;
        n -= (seed >> 9) % 64u < n ? (seed >> 9) % 64u : 0;
        f = (file_t){ g, n, 0 };
        lyrics_tag(rd, &f, n, k ? TAGS_MP3 : TAGS_FLAC, l.text, (uint32_t)sizeof l.text);
        for (uint32_t i = 0; i < 300; i++) {
            seed = seed * 1103515245u + 12345u;
            l.text[i] = marks[(seed >> 16) % 25u];
        }
        lyrics_parse(&l, 300);
        for (uint32_t i = 0; i < l.n; i++)
            CHECK(l.at[i] < 300u && strlen(l.text + l.at[i]) < 300u);
        for (uint32_t i = 1; l.synced && i < l.n; i++)
            CHECK(l.ds[i - 1] <= l.ds[i]);
        if (fails)
            return;
    }
}

int main(void)
{
    test_flac();
    test_mp3();
    test_parse();
    test_fuzz();
    if (fails) {
        printf("test_lyrics: %d FAILED\n", fails);
        return 1;
    }
    printf("test_lyrics: ok\n");
    return 0;
}
