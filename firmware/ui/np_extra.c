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
#include "services/config.h"
#include "services/queue.h"
#include "ui/frame.h"
#include "ui/np_extra.h"
#include "ui/ui.h"
#include "ui/layout.h"
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

static int line_at(uint32_t pos_ms, uint32_t dur_ms)
{
    if (!s_ly.n)
        return -1;
    if (s_ly.synced)
        return lyrics_line(&s_ly, pos_ms);
    /* plain lyrics: spread over the song */
    return dur_ms ? (int)((uint64_t)(pos_ms < dur_ms ? pos_ms : dur_ms - 1u) * s_ly.n / dur_ms) : 0;
}

/* The views from the layout's [lyrics] and [next] (docs/LAYOUTS.md; ui/nav.c lay_screen_draw draws them,
 * startup/ap_ovl.c ap_npx_draw parsed the section before this overlay took the window): their words. */
static int s_k;                        /* the lyrics line now (-1: before the first) */

static const char *view_words(const lay_el_t *e, char *b, int cap)
{
    int px = e->font == LFN_PX, at = (int)e->a - 8;

    b[0] = 0;
    switch (e->field) {
    case LF_TITLE: return g_np.title;
    case LF_ARTIST: return g_np.artist;
    case LF_ALBUM: return g_np.album;
    case LF_SONG:                      /* (the pixel font has no "Â·") */
        if (!g_np.artist[0])
            return g_np.title;
        snprintf(b, (unsigned)cap, px ? "%s - %s" : "%s \xC2\xB7 %s", g_np.title, g_np.artist);
        return b;
    case LF_LYRIC: {                   /* (an instrumental gap: "â¢  â¢  â¢", "..." in the pixel font) */
        int n = s_k + at;
        const char *l;

        if (n < 0 || n >= (int)s_ly.n || (!at && s_k < 0))
            return "";
        l = s_ly.text + s_ly.at[n];
        return l[0] ? l : px ? "..." : "\xE2\x80\xA2  \xE2\x80\xA2  \xE2\x80\xA2";
    }
    case LF_NOLYRICS: return s_ly.n ? "" : px ? "No lyrics" : "No lyrics in this song's tags";
    case LF_AUDIO: snprintf(b, (unsigned)cap, "%s  %s", g_np.codec, g_np.fmt); return b;
    case LF_MODE: return queue_mode() == Q_SHUFFLE ? "Shuffle" : queue_mode() ? "Repeat" : "";
    case LF_UPNEXT: return at >= 0 && at < X.nn ? X.next[at] : "";
    case LF_UPNUM:
        if (at >= 0 && at < X.nn)
            snprintf(b, (unsigned)cap, "%d", at + 1);
        return b;
    case LF_ENDQUEUE: return X.nn ? "" : "End of the queue";
    }
    return b;
}

void npx_draw(int v)
{
    lay_ctx_t x = { view_words, 0, 0, 0, NO_ICON, 0, T->text, BAND_Y - 1 };

    lyrics_load();                     /* (X.clus: the track, for the queue's key too) */
    if (v == NPV_LYRICS)
        s_k = X.line = (int16_t)line_at((uint32_t)player_get_position_ms(), g_np.dur * 1000u);
    else if (v == NPV_NEXT)
        next_load();
    else
        return;
    lay_screen_draw(v == NPV_LYRICS ? LS_LYRICS : LS_NEXT, &x, 0);
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
        splash_text(&font_d28, 92, "GILB", 4, 0xFFFF);   /* (display_text: a byte count, not -1) */
        splash_text(&font_b13, 118, __DATE__, 11, 0x8410);
    }
    display_flush_rows(0, DISPLAY_H - 1);
    if (s_splash)
        fs_close(s_splash);
    s_splash = NULL;
    display_clear(0x0000);             /* (the panel keeps the picture until the first frame) */
}

