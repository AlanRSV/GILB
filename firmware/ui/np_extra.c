/*
 * np_extra.c -- Now Playing's Lyrics and Up Next views (np_extra.h), in the npx overlay: only
 * loaded while one of them is on screen. Its .bss (the lyrics, the names) is resident like every
 * overlay's, so what was read survives the window going to another overlay meanwhile.
 *
 * Lyrics: the track's tags (services/lyrics.c), else a .lrc file of the same name beside it.
 * Read once per track, when the view first needs them. Up Next: the queue's next tracks by file
 * name (what the Folders view shows), read again when the track, the queue or its mode changes.
 *
 * Also the boot splash (np_extra.h SPLASH_PATH): here rather than resident for the code budget; the
 * overlay is loaded once at boot for it.
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "audio/audio.h"
#include "drivers/display.h"
#include "drivers/text.h"
#include "services/fs.h"
#include "services/lyrics.h"
#include "services/queue.h"
#include "ui/frame.h"
#include "ui/np_extra.h"
#include "ui/ui.h"
#include "player.h"

#define BAND_Y 138                     /* ui/screens.c: the band starts here */
#define NEXT_ROWS 6

static lyrics_t s_ly;
static fs_file_t *s_splash;            /* the picture, open while the splash is flushed */
static struct {
    uint32_t clus;                     /* the track s_ly is for (its first cluster; 0: none yet) */
    uint32_t nkey;                     /* the queue state s_next is for */
    uint8_t nn;
    int16_t line;                      /* the lyrics line drawn last */
    char next[NEXT_ROWS][48];
} X;

/* ---- reading ---------------------------------------------------------------------------- */

static uint32_t rd_at(void *u, uint32_t off, void *buf, uint32_t n)
{
    long r;

    if (fs_seek((fs_file_t *)u, (long)off) < 0)
        return 0;
    r = fs_read((fs_file_t *)u, buf, n);
    return r > 0 ? (uint32_t)r : 0;
}

static int kind_of(const char *name)
{
    const char *d = strrchr(name, '.');

    if (!d)
        return -1;
    if (!strcasecmp(d, ".flac"))
        return TAGS_FLAC;
    if (!strcasecmp(d, ".mp3"))
        return TAGS_MP3;
    if (!strcasecmp(d, ".wav"))
        return TAGS_WAV;
    return -1;
}

typedef struct { const char *stem; uint32_t len; fs_ref_t ref; } lrc_find_t;

static int lrc_find(void *u, const char *name, const fs_ref_t *r)
{
    lrc_find_t *f = u;

    if ((r->flags & FS_REF_DIR) || strncasecmp(name, f->stem, f->len) || strcasecmp(name + f->len, ".lrc"))
        return 0;
    f->ref = *r;
    return 1;
}

/* The .lrc beside the track (same name, .lrc): its text into s_ly.text, its length (0: none). */
static uint32_t lrc_file(const fs_ref_t *t, const char *name)
{
    fs_ref_t d = { .clus = t->pclus, .size = t->psize, .vol = t->vol,
                   .flags = (uint8_t)(FS_REF_DIR | ((t->flags & FS_REF_PCONTIG) ? FS_REF_CONTIG : 0)) };
    const char *dot = strrchr(name, '.');
    lrc_find_t f = { name, dot ? (uint32_t)(dot - name) : (uint32_t)strlen(name), { 0 } };
    fs_file_t *h;
    long n;

    if (fs_scan_ref(&d, lrc_find, &f) < 0 || !f.ref.size || !(h = fs_open_ref(&f.ref)))
        return 0;
    n = fs_read(h, s_ly.text, LYR_TEXT - 1u);
    fs_close(h);
    if (n <= 0)
        return 0;
    if (n >= 3 && !memcmp(s_ly.text, "\xEF\xBB\xBF", 3)) {
        memmove(s_ly.text, s_ly.text + 3, (size_t)n - 3u);
        n -= 3;
    }
    s_ly.text[n] = 0;
    return (uint32_t)strlen(s_ly.text);
}

/* The playing track's lyrics into s_ly, once per track. */
static void lyrics_load(void)
{
    audio_status_t st;
    char name[FS_NAME_MAX];
    uint32_t n = 0;
    fs_file_t *f;
    int kind;

    audio_status(&st);
    if (!st.seq || st.track.ref.clus == X.clus)
        return;
    X.clus = st.track.ref.clus;
    s_ly.n = 0;
    s_ly.synced = 0;
    if (fs_ref_name(&st.track.ref, name, sizeof name) != 0)
        return;
    kind = kind_of(name);
    if (kind >= 0 && (f = fs_open_ref(&st.track.ref)) != NULL) {
        long size = fs_file_size(f);

        if (size > 0)
            n = lyrics_tag(rd_at, f, (uint32_t)size, kind, s_ly.text, LYR_TEXT);
        fs_close(f);
    }
    if (!n)
        n = lrc_file(&st.track.ref, name);
    lyrics_parse(&s_ly, n);
}

/* A file name as a title: the extension and a leading track number ("03 - ", "3. ") dropped. */
static void title_of(const char *name, char *out, int cap)
{
    const char *p = name, *dot = strrchr(name, '.');
    int len, nd = 0;

    while (*p >= '0' && *p <= '9' && nd < 3)
        p++, nd++;
    if (nd && (*p == '.' || *p == ' ' || *p == '-' || *p == '_') && p[1] && p != dot) {
        while (*p == '.' || *p == ' ' || *p == '-' || *p == '_')
            p++;
    } else {
        p = name;
    }
    len = dot && dot > p ? (int)(dot - p) : (int)strlen(p);
    if (len > cap - 1)
        len = cap - 1;
    memcpy(out, p, (size_t)len);
    out[len] = 0;
}

static void next_load(void)
{
    int cur = queue_pos(), cnt = queue_count(), mode = queue_mode();
    uint32_t key = (uint32_t)cur * 2654435761u ^ (uint32_t)cnt << 16 ^ (uint32_t)mode << 28 ^ X.clus;
    char name[FS_NAME_MAX];

    if (key == X.nkey && X.nkey)
        return;
    X.nkey = key ? key : 1u;
    X.nn = 0;
    for (int k = 1; X.nn < NEXT_ROWS && cur >= 0 && k < cnt; k++) {
        int q = cur + k;

        if (q >= cnt) {
            if (mode != Q_REPEAT_ALL)
                break;
            q -= cnt;
        }
        if (!queue_ref(q) || fs_ref_name(queue_ref(q), name, sizeof name) != 0)
            continue;
        title_of(name, X.next[X.nn], (int)sizeof X.next[0]);
        X.nn++;
    }
}

/* ---- drawing ---------------------------------------------------------------------------- */

/* text centred on x 8..312, cut to fit */
static void centre(const font_t *f, int base, const char *s, uint16_t c)
{
    ui_text(f, 8, base, 304, 1, s, c, BODY_Y, BAND_Y - 1);
}

static const char *line_text(int i)
{
    const char *s;

    if (i < 0 || i >= (int)s_ly.n)
        return "";
    s = s_ly.text + s_ly.at[i];
    return s[0] ? s : "\xE2\x80\xA2  \xE2\x80\xA2  \xE2\x80\xA2";   /* an instrumental gap: "•  •  •" */
}

static int line_at(uint32_t pos_ms, uint32_t dur_ms)
{
    if (!s_ly.n)
        return -1;
    if (s_ly.synced)
        return lyrics_line(&s_ly, pos_ms);
    /* plain lyrics: spread over the song */
    return dur_ms ? (int)((uint64_t)(pos_ms < dur_ms ? pos_ms : dur_ms - 1u) * s_ly.n / dur_ms) : 0;
}

static void draw_lyrics(void)
{
    const theme_t *t = T;
    char head[170];
    int k;

    lyrics_load();
    k = X.line = (int16_t)line_at((uint32_t)player_get_position_ms(), g_np.dur * 1000u);
    if (g_np.artist[0])
        snprintf(head, sizeof head, "%s \xC2\xB7 %s", g_np.title, g_np.artist);
    else
        snprintf(head, sizeof head, "%s", g_np.title);
    centre(&font_b13, 36, head, t->text_dim);
    ui_fill(40, 44, 240, 1, t->divider);
    if (!s_ly.n) {
        centre(&font_b13, 92, "No lyrics in this song's tags", t->text_dim);
        return;
    }
    {
        const char *cur = k < 0 ? "" : line_text(k);
        text_line_t ln[2];
        int n = text_wrap(&font_t17, cur, -1, 300, 2, ln);
        char b[160];

        if (k > 0)
            centre(&font_b13, 64, line_text(k - 1), t->text_dim);
        for (int i = 0; i < n; i++) {
            int len = ln[i].len < (int)sizeof b - 1 ? ln[i].len : (int)sizeof b - 1;

            memcpy(b, cur + ln[i].off, (size_t)len);
            b[len] = 0;
            centre(&font_t17, n == 2 ? 88 + 20 * i : 96, b, t->text);
        }
        if (k + 1 < (int)s_ly.n)
            centre(&font_b13, 128, line_text(k + 1), t->text_dim);
    }
}

static void draw_next(void)
{
    const theme_t *t = T;
    text_line_t ln[2];
    int n = text_wrap(&font_t17, g_np.title, -1, 172, 2, ln), y;
    char b[100];

    lyrics_load();                     /* (X.clus: the track, for the queue's key) */
    next_load();
    for (int i = 0; i < n; i++) {
        int len = ln[i].len < (int)sizeof b - 1 ? ln[i].len : (int)sizeof b - 1;

        memcpy(b, g_np.title + ln[i].off, (size_t)len);
        b[len] = 0;
        ui_text(&font_t17, 8, 44 + 20 * i, 172, 0, b, t->text, BODY_Y, BAND_Y - 1);
    }
    y = n == 2 ? 86 : 66;
    ui_text(&font_b13, 8, y, 172, 0, g_np.artist, t->text, BODY_Y, BAND_Y - 1);
    ui_text(&font_b13, 8, y + 18, 172, 0, g_np.album, t->text_dim, BODY_Y, BAND_Y - 1);
    snprintf(b, sizeof b, "%s  %s", g_np.codec, g_np.fmt);
    ui_text(&font_s8, 8, 130, 172, 0, b, t->text_dim, BODY_Y, BAND_Y - 1);

    ui_fill(188, BODY_Y + 6, 1, BAND_Y - BODY_Y - 12, t->divider);
    ui_text(&font_s8, 196, 34, 116, 0, "UP NEXT", t->accent, BODY_Y, BAND_Y - 1);
    if (queue_mode())
        ui_text(&font_s8, 196, 34, 116, 2, queue_mode() == Q_SHUFFLE ? "SHUFFLE" : "REPEAT", t->text_dim,
                BODY_Y, BAND_Y - 1);
    if (!X.nn)
        ui_text(&font_b13, 196, 58, 116, 0, "End of the queue", t->text_dim, BODY_Y, BAND_Y - 1);
    for (int i = 0; i < X.nn; i++)
        ui_text(&font_b13, 196, 54 + 16 * i, 116, 0, X.next[i], i ? t->text_dim : t->text, BODY_Y, BAND_Y - 1);
}

void npx_draw(int v)
{
    if (v == NPV_LYRICS)
        draw_lyrics();
    else if (v == NPV_NEXT)
        draw_next();
}

/* From Now Playing's poll while the Lyrics view is up: redrawn when another line is due. */
void npx_tick(void)
{
    if (g_np.seq && g_np.seq != 0xFFFFFFFFu &&    /* (nothing playing, or starting: nothing drawn) */
        line_at((uint32_t)player_get_position_ms(), g_np.dur * 1000u) != X.line)
        ui_invalidate(0, BODY_Y, DISPLAY_W - 1, BAND_Y - 1);
}

/* ---- the boot splash ------------------------------------------------------------------------ */

/* Each band's rows straight from the file into the band (no picture in RAM). */
static void splash_band(const display_band_t *b)
{
    for (int y = b->ya; y <= b->yb; y++) {
        uint16_t *row = b->band + (y - b->band_y) * DISPLAY_W;

        if (!s_splash || fs_seek(s_splash, (long)y * DISPLAY_W * 2) < 0 ||
            fs_read(s_splash, row, DISPLAY_W * 2u) != (long)(DISPLAY_W * 2u))
            memset(row, 0, DISPLAY_W * 2u);
    }
}

static void splash_text(const font_t *f, int base, const char *s, int n, uint16_t c)
{
    display_text(f, 0, 0, DISPLAY_W - 1, DISPLAY_H - 1, (DISPLAY_W - text_width(f, s, n)) / 2, base, s, n, c);
}

/* SPLASH_PATH if it is there and the right size, else RECHORD and the build's date (as About shows
 * it). The picture's op leaves the list once flushed: nothing may call into this overlay later. */
void npx_splash(void)
{
    display_clear(0x0000);
    s_splash = fs_open_read(SPLASH_PATH);
    if (s_splash && fs_file_size(s_splash) == (long)SPLASH_BYTES) {
        display_custom(0, 0, DISPLAY_W - 1, DISPLAY_H - 1, splash_band, 0, 1);
    } else {
        splash_text(&font_d28, 92, "RECHORD", 7, 0xFFFF);   /* (display_text: a byte count, not -1) */
        splash_text(&font_b13, 118, __DATE__, 11, 0x8410);
    }
    display_flush_rows(0, DISPLAY_H - 1);
    if (s_splash)
        fs_close(s_splash);
    s_splash = NULL;
    display_clear(0x0000);             /* (the panel keeps the picture until the first frame) */
}

