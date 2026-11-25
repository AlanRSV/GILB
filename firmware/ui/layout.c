/*
 * layout.c -- screens drawn from layouts (docs/LAYOUTS.md): the parser, the drawing and the built-in
 * layouts, in the lay overlay (startup/ap_ovl.c ap_lay_np / ap_lay_scan, firmware_ap.ld). Phase 1:
 * Now Playing's cover view.
 *
 * A layout is text, one element a line ("text 126 48 186 field=title font=t17"); the built-ins are the
 * Looks' own screens written that way (k_modern, k_retro). The Look's Layout setting names a file of
 * GILB/LAYOUTS/ ("" = Built-in): its [np] section, read a chunk at a time (no buffer for the whole
 * file), replaces the built-in's. The parsed elements are main memory's (g_lay, ui/layout.h); this
 * overlay draws them with the UI's own calls, in the theme's colours.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "frame.h"
#include "layout.h"
#include "np_extra.h"
#include "drivers/display.h"
#include "drivers/text.h"
#include "services/config.h"
#include "services/fs.h"
#include "services/queue.h"
#include "theme/theme.h"

#define BAND_Y 138                       /* (ui/screens.c's: the volume and seek bands' rows) */
#define MINUS "\xE2\x88\x92"

unsigned retro_year(void);               /* ui/retro_tags.c (this overlay) */

/* ---- the built-in layouts: the Looks' Now Playing as they were drawn by hand ---------------------- */

static const char k_modern[] =
    "[np]\n"
    "cover 6 24 112 noart=hue pause=1\n"
    "text 126 48 186 field=title font=t17 wrap=2 up=6 grow=12\n"
    "text 126 70 186 field=artist font=b13 flow=1\n"
    "text 126 88 186 field=album font=b13 color=text_dim flow=1\n"
    "badge 126 108 field=codec style=fill hires=1\n"
    "badge -1 108 field=format style=outline hires=1\n"
    "text 126 130 186 field=facts font=s8 color=text_dim\n"
    "bar 8 142 304 4 kind=progress style=line\n"
    "text 8 163 62 field=elapsed font=b13\n"
    "text 232 163 80 field=remaining font=b13 align=r color=text_dim\n"
    "text 72 161 156 field=next font=s8 align=c color=text_dim\n"
    "[list]\n"
    "row 21\n"
    "select 0 0 316 21 color=sel_bg\n"
    "mark 6 5 numx=2 numw=18 numy=15 numfont=s8\n"
    "value 24 15 286 font=b13 color=value gap=8\n"
    "label 24 15 286 font=b13 bold=m13\n"
    "chev 304 5 take=10\n"
    "scroll 316\n"
    "[header]\n"
    "part battery\n"
    "part percent font=s8 y=13 gap=3\n"
    "part clock font=s8 y=13 gap=8\n"
    "part volume font=b13 y=14 gap=8 color=text\n"
    "part output gap=13\n"
    "part lock gap=15 color=warn\n"
    "part play font=b13 y=14 gap=8\n"
    "title font=m13 y=14\n";

static const char k_retro[] =
    "[np]\n"
    "box 3 22 314 145 color=panel\n"
    "text 8 37 160 field=queue font=px\n"
    "cover 8 42 84 tint=1 noart=solid color=text\n"
    "text 102 58 210 field=title font=px alt=t17\n"
    "text 102 80 210 field=artist font=px alt=t17\n"
    "text 102 102 210 field=album font=px alt=t17\n"
    "text 102 124 210 field=year font=px alt=t17\n"
    "bar 8 139 304 10 kind=progress style=outline color=bar_fill track=text\n"
    "text 8 167 62 field=elapsed font=px\n"
    "text 232 167 80 field=remaining font=px align=r\n"
    "text 72 164 158 field=kbps font=s8 align=c\n"
    "[list]\n"
    "row 21\n"
    "select 0 0 312 21 color=sel_bg\n"
    "mark 6 5 numx=0 numw=24 numy=16 numfont=px icons=0 color=text on=text\n"
    "value 8 16 298 font=px color=text gap=10 max=140\n"
    "label 8 16 298 font=px after=6\n"
    "chev 299 16 style=text take=14 font=px color=text\n"
    "scroll 313 style=outline\n"
    "[header]\n"
    "part battery\n"
    "part percent font=px alt=s8 y=15 gap=3\n"
    "part clock font=s8 y=13 gap=8\n"
    "part volume font=b13 y=14 gap=8 color=text\n"
    "part output gap=13\n"
    "part lock gap=15 color=warn\n"
    "part play font=b13 y=14 gap=8\n"
    "title font=px alt=m13 y=15 np=volume\n";

/* ---- names ------------------------------------------------------------------------------------- */

static const char *const k_kinds[] = { "", "text", "cover", "box", "frame", "bar", "badge",
                                       "mark", "label", "value", "chev", "select", "scroll", "part", "title" };
static const uint8_t k_need[] = { 0, 3, 3, 4, 4, 4, 2, 2, 3, 3, 2, 4, 1, 0, 0 };   /* its numbers */
static const char *const k_parts[] = { "battery", "percent", "clock", "volume", "output", "lock", "play" };
static const char *const k_fields[] = { "", "title", "artist", "album", "year", "queue", "elapsed", "remaining",
                                        "total", "kbps", "facts", "next", "codec", "format", "progress", "volume" };
static const char *const k_fonts[] = { "s8", "b13", "m13", "t17", "d28", "px" };
static const font_t *const k_font[] = { &font_s8, &font_b13, &font_m13, &font_t17, &font_d28, &font_px };

/* colour roles: the theme's fields (by their place in theme_t), and the panel */
#define TOFF(f) (uint8_t)(offsetof(theme_t, f) / 2u)
static const struct { const char *name; uint8_t off; } k_roles[] = {
    { "text", TOFF(text) }, { "text_dim", TOFF(text_dim) }, { "text_off", TOFF(text_off) },
    { "accent", TOFF(accent) }, { "on_accent", TOFF(on_accent) }, { "background", TOFF(background) },
    { "surface", TOFF(surface) }, { "divider", TOFF(divider) }, { "sel_bg", TOFF(sel_bg) },
    { "sel_text", TOFF(sel_text) }, { "bar_fill", TOFF(bar_fill) }, { "bar_track", TOFF(bar_track) },
    { "badge_bg", TOFF(badge_bg) }, { "badge_text", TOFF(badge_text) }, { "hires", TOFF(hires) },
    { "danger", TOFF(danger) }, { "success", TOFF(success) }, { "warn", TOFF(warn) }, { "value", TOFF(value) },
    { "scroll", TOFF(scroll) }, { "panel", LCOL_PANEL },
};
#define NROLES ((int)(sizeof k_roles / sizeof k_roles[0]))
#define DEF LCOL_DEF
#define role lay_color                   /* (main memory's: the lists use it too) */

/* ---- the parser -------------------------------------------------------------------------------- */

static int pick(const char *s, int n, const char *const *names, int count)
{
    for (int i = 0; i < count; i++)
        if ((int)strlen(names[i]) == n && !memcmp(s, names[i], (size_t)n))
            return i;
    return -1;
}

static int pick_role(const char *s, int n)   /* its place in theme_t (layout.h LCOL_*), -1 */
{
    for (int i = 0; i < NROLES; i++)
        if ((int)strlen(k_roles[i].name) == n && !memcmp(s, k_roles[i].name, (size_t)n))
            return k_roles[i].off;
    return -1;
}

static unsigned s_strn;                  /* bytes of g_lay_str used */

/* One line into g_lay ([np]) or g_lrow ([list]) (ignored when it is not an element this build knows). */
static void parse_line(const char *p, int list)   /* list: 0 [np], 1 [list], 2 [header] */
{
    lay_el_t e;
    int num[4] = { 0 }, nn = 0, need, k, i0;
    const char *q;

    while (*p == ' ' || *p == '\t')
        p++;
    for (q = p; *q && *q != ' ' && *q != '\t'; q++)
        ;
    if (list == 1 && q - p == 3 && !memcmp(p, "row", 3)) {   /* a list row's height */
        int h = atoi(q);

        g_lrow_h = (uint8_t)(h < LROW_MIN ? LROW_MIN : h > LROW_MAXH ? LROW_MAXH : h);
        return;
    }
    if ((k = pick(p, (int)(q - p), k_kinds, 15)) <= 0 ||
        (list == 2 ? k < LK_PART || g_lhdr_n >= LHDR_MAX :
         k >= LK_PART || (k >= LK_MARK ? !list : list && k != LK_BOX && k != LK_FRAME)) ||
        (list == 1 ? g_lrow_n >= LROW_MAX : !list && g_lay_n >= LAY_MAX))
        return;                          /* (boxes and frames: [np]'s and [list]'s) */
    memset(&e, 0, sizeof e);
    e.kind = (uint8_t)k;
    e.font = LFN_B13;
    e.color = e.color2 = DEF;
    need = k_need[k];
    p = q;
    if (k == LK_PART) {                  /* part <name> */
        while (*p == ' ')
            p++;
        for (q = p; *q && *q != ' '; q++)
            ;
        if ((i0 = pick(p, (int)(q - p), k_parts, 7)) < 0)
            return;
        e.field = (uint8_t)i0;
        p = q;
    }
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (nn < need && (*p == '-' || (*p >= '0' && *p <= '9'))) {   /* (the firmware's libc has no strtol) */
            int neg = *p == '-', v = 0;

            for (p += neg; *p >= '0' && *p <= '9'; p++)
                v = v * 10 + (*p - '0');
            num[nn++] = neg ? -v : v;
            continue;
        }
        {
            const char *key = p, *eq, *v, *ve;

            for (eq = p; *eq && *eq != '=' && *eq != ' '; eq++)
                ;
            if (*eq != '=') {            /* (a word that is not key=value: skipped) */
                for (p = eq; *p && *p != ' '; p++)
                    ;
                continue;
            }
            v = eq + 1;
            if (*v == '"') {
                for (ve = ++v; *ve && *ve != '"'; ve++)
                    ;
                p = *ve ? ve + 1 : ve;
            } else {
                for (ve = v; *ve && *ve != ' ' && *ve != '\t'; ve++)
                    ;
                p = ve;
            }
            int kn = (int)(eq - key), vn = (int)(ve - v), i;

#define KEY(s) (kn == (int)sizeof(s) - 1 && !memcmp(key, s, (size_t)kn))
#define VAL(s) (vn == (int)sizeof(s) - 1 && !memcmp(v, s, (size_t)vn))
            if (KEY("field") && (i = pick(v, vn, k_fields, 16)) > 0)
                e.field = (uint8_t)i;
            else if (KEY("kind") && (i = pick(v, vn, k_fields, 16)) > 0)
                e.field = (uint8_t)i;
            else if (KEY("text") && s_strn + (unsigned)vn + 1u <= LAY_STR) {
                e.field = LF_WORDS;
                e.b = (uint8_t)s_strn;
                memcpy(g_lay_str + s_strn, v, (size_t)vn);
                g_lay_str[s_strn + (unsigned)vn] = 0;
                s_strn += (unsigned)vn + 1u;
            } else if (KEY("font") && (i = pick(v, vn, k_fonts, 6)) >= 0)
                e.font = (uint8_t)i;
            else if (KEY("align"))
                e.flags = (uint8_t)((e.flags & ~LG_ALIGN) | (VAL("c") ? 1u : VAL("r") ? 2u : 0u));
            else if (KEY("color") && (i = pick_role(v, vn)) >= 0)
                e.color = (uint8_t)i;
            else if (KEY("track") && (i = pick_role(v, vn)) >= 0)
                e.color2 = (uint8_t)i;
            else if (KEY("wrap") && VAL("2"))
                e.flags |= LG_WRAP;
            else if (KEY("up"))
                e.a = (uint8_t)atoi(v);
            else if (KEY("grow"))
                e.b = (uint8_t)atoi(v);
            else if (KEY("flow") && VAL("1"))
                e.flags |= LG_FLOW;
            else if (KEY("tint") && VAL("1"))
                e.flags |= LG_TINT;
            else if (KEY("pause") && VAL("1"))
                e.flags |= LG_PAUSE;
            else if (KEY("noart") && VAL("solid"))
                e.flags |= LG_SOLID;
            else if (KEY("alt") && k >= LK_PART && (i = pick(v, vn, k_fonts, 6)) >= 0)
                e.b = (uint8_t)(i + 1);  /* (a header part's font when px lacks a letter) */
            else if ((KEY("style") && (VAL("outline") || VAL("text"))) || (KEY("alt") && VAL("t17")) ||
                     (KEY("np") && VAL("volume")))
                e.flags |= LG_ALT;
            else if (KEY("y") && k >= LK_PART)
                num[1] = atoi(v), nn = nn > 1 ? nn : 2;
            else if (KEY("gap") && k >= LK_PART)
                e.a = (uint8_t)atoi(v);
            else if (KEY("on") && (i = pick_role(v, vn)) >= 0)
                e.color2 = (uint8_t)i;
            else if (KEY("numx"))
                e.w = (int16_t)atoi(v);  /* (mark: its number's box) */
            else if (KEY("numw"))
                e.h = (int16_t)atoi(v);
            else if (KEY("numy"))
                e.a = (uint8_t)atoi(v);  /* (mark: its number's baseline) */
            else if (KEY("bold") && (i = pick(v, vn, k_fonts, 6)) >= 0)
                e.a = (uint8_t)(i + 1);  /* (label: a bold row's font) */
            else if (KEY("numfont") && (i = pick(v, vn, k_fonts, 6)) >= 0)
                e.font = (uint8_t)i;
            else if (KEY("icons") && VAL("0"))
                e.flags |= LG_SOLID;
            else if (KEY("gap") || KEY("take"))
                e.a = (uint8_t)atoi(v);
            else if (KEY("after") || KEY("max"))
                e.b = (uint8_t)atoi(v);
            else if (KEY("hires") && VAL("1"))
                e.color2 = 0xFE;         /* (badges: the hi-res colours when the track is) */
#undef KEY
#undef VAL
        }
    }
    if (nn < need)
        return;
    e.x = (int16_t)num[0];
    e.y = (int16_t)(nn > 1 ? num[1] : 0);
    if (nn > 2)
        e.w = (int16_t)num[2];
    if (nn > 3 || k == LK_COVER)
        e.h = (int16_t)(nn > 3 ? num[3] : num[2]);
    if (k == LK_BAR && !e.field)
        e.field = LF_PROGRESS;
    if (k >= LK_PART && !num[1])
        e.y = 14;                        /* (the header's baseline) */
    if (list == 2)
        g_lhdr[g_lhdr_n++] = e;
    else if (list)
        g_lrow[g_lrow_n++] = e;
    else
        g_lay[g_lay_n++] = e;
}

/* A layout's [np] section, its text from get(): one line at a time. */
typedef struct { const char *mem; fs_file_t *f; char buf[256]; int n, at, eof; } src_t;

static int src_line(src_t *s, char *line, int cap)
{
    int k = 0, any = 0;

    for (;;) {
        char c;

        if (s->mem) {
            if (!*s->mem)
                break;
            c = *s->mem++;
        } else {
            if (s->at >= s->n) {
                long r = s->eof ? 0 : fs_read(s->f, s->buf, sizeof s->buf);

                if (r <= 0) {
                    s->eof = 1;
                    break;
                }
                s->n = (int)r;
                s->at = 0;
            }
            c = s->buf[s->at++];
        }
        any = 1;
        if (c == '\n')
            break;
        if (c != '\r' && k < cap - 1)
            line[k++] = c;
    }
    line[k] = 0;
    return any;
}

/* The sections of s (but those in skip: 1 [np], 2 [list], 4 [header]) into g_lay / g_lrow / g_lhdr;
 * returns the ones it had. */
static int parse(src_t *s, int skip)
{
    char line[160];
    int sec = 0, had = 0;

    while (src_line(s, line, sizeof line)) {
        char *h = strchr(line, '#');

        if (h && (h == line || h[-1] == ' ' || h[-1] == '\t') && !(h > line && strchr(line, '"') && strchr(line, '"') < h))
            *h = 0;                      /* (a comment: # at the start or after a space, not in quotes) */
        if (line[0] == '[') {
            sec = !strncmp(line, "[np]", 4) ? 1 : !strncmp(line, "[list]", 6) ? 2 : !strncmp(line, "[header]", 8) ? 4 : 0;
            if (sec & skip)
                sec = 0;
            else if (sec == 1)
                g_lay_n = 0, s_strn = 0;
            else if (sec == 2)
                g_lrow_n = 0, g_lrow_h = 21;
            else if (sec == 4)
                g_lhdr_n = 0;
            had |= sec;
            continue;
        }
        if (sec)
            parse_line(line, sec == 4 ? 2 : sec == 2);
    }
    return had;
}

static uint32_t hash(const char *s)
{
    uint32_t h = 2166136261u;

    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h & 0xFFFFu;
}

/* The Look's layout parsed into g_lay and g_lrow if it is not already: the file's sections, the
 * built-in's for those it does not have. */
int lay_load(void)
{
    int look = g_cfg.look ? 1 : 0, had = 0;
    const char *name = g_cfg.layout[look];
    int32_t key = (int32_t)((uint32_t)look << 16 | hash(name));
    src_t s;

    if (g_lay_key == key && g_lay_n)
        return 1;
    g_lay_key = key;
    memset(&s, 0, sizeof s);
    if (name[0]) {
        char path[64];

        snprintf(path, sizeof path, LAY_DIR "/%s.txt", name);
        if ((s.f = fs_open_read(path)) != NULL) {
            had = parse(&s, 0);
            fs_close(s.f);
        }
    }
    memset(&s, 0, sizeof s);
    s.mem = look ? k_retro : k_modern;
    parse(&s, had);
    return 1;
}

/* ---- the drawing ------------------------------------------------------------------------------- */

static const uint16_t k_hues[8] = {     /* the monogram tile's hues (ui/screens.c's) */
    0x7A7A, 0x2C70, 0xC287, 0x3B78, 0xAA2B, 0x5C65, 0xB3C3, 0x4AF5
};

static struct { int16_t x, y, s; uint8_t tint; uint16_t pal[32]; } C;   /* the cover being drawn */

static void cover_band(const display_band_t *b)          /* 56 x 56 to C.s, nearest */
{
    for (int y = b->ya; y <= b->yb; y++) {
        const uint16_t *src = g_np_cover + (y - C.y) * 56 / C.s * 56;
        uint16_t *dst = b->band + (y - b->band_y) * DISPLAY_W;

        for (int x = b->cx0; x <= b->cx1; x++) {
            uint16_t p = src[(x - C.x) * 56 / C.s];

            if (C.tint) {
                unsigned l = ((p >> 11) * 8u * 77u + ((p >> 5) & 63u) * 4u * 150u + (p & 31u) * 8u * 29u) >> 8;

                p = C.pal[l >> 3];
            }
            dst[x] = p;
        }
    }
}

static void fmt_t(char *o, int cap, uint32_t s)
{
    if (s >= 3600u)
        snprintf(o, (unsigned)cap, "%u:%02u:%02u", (unsigned)(s / 3600u), (unsigned)(s / 60u % 60u), (unsigned)(s % 60u));
    else
        snprintf(o, (unsigned)cap, "%u:%02u", (unsigned)(s / 60u), (unsigned)(s % 60u));
}

#define N_TRACK() (g_np.seq && g_np.seq != 0xFFFFFFFFu)

/* An element's words now (into b); "" when there are none to show. */
static const char *words(const lay_el_t *e, char *b, int cap)
{
    b[0] = 0;
    switch (e->field) {
    case LF_WORDS: return g_lay_str + e->b;
    case LF_TITLE: return g_np.title;
    case LF_ARTIST: return g_np.artist;
    case LF_ALBUM: return g_np.album;
    case LF_CODEC: return g_np.codec;
    case LF_FORMAT: return g_np.fmt;
    case LF_FACTS: return g_np.facts;
    case LF_YEAR: {
        unsigned y = N_TRACK() ? retro_year() : 0u;

        if (y)
            snprintf(b, (unsigned)cap, "%u", y);
        return b;
    }
    case LF_QUEUE:
        if (queue_count() > 0 && N_TRACK())
            snprintf(b, (unsigned)cap, "%d of %d", queue_pos() + 1, queue_count());
        return b;
    case LF_ELAPSED:
        fmt_t(b, cap, g_np.pos);
        return b;
    case LF_TOTAL:
        if (g_np.dur)
            fmt_t(b, cap, g_np.dur);
        return b;
    case LF_REMAIN:
        if (g_np.dur) {                  /* (the pixel font has no U+2212: its "-") */
            int m = e->font == 5 ? 1 : 3;

            memcpy(b, e->font == 5 ? "-" : MINUS, (size_t)m);
            fmt_t(b + m, cap - m, g_np.dur > g_np.pos ? g_np.dur - g_np.pos : 0);
        }
        return b;
    case LF_KBPS: {
        unsigned kb = (unsigned)atoi(g_np.facts);   /* ("1411 KBPS · 41.6 MB": np_load) */
        const char *c = g_np.codec;

        if (kb && strchr(g_np.facts, 'K')) {        /* (lossless and Vorbis/Opus are variable by nature) */
            int vbr = !strcmp(c, "FLAC") || !strcmp(c, "ALAC") || !strcmp(c, "APE") || !strcmp(c, "WV") ||
                      !strcmp(c, "OGG") || !strcmp(c, "OPUS");

            snprintf(b, (unsigned)cap, vbr ? "%u kbps vbr" : "%u kbps", kb);
        }
        return b;
    }
    case LF_NEXT:
        if (g_np.next[0])
            snprintf(b, (unsigned)cap, "NEXT  %s", g_np.next);
        return b;
    }
    return b;
}

static int s_grow, s_badge_x;

static void draw_text(const lay_el_t *e)
{
    char b[110], line[100];
    const char *s = words(e, b, sizeof b);
    const font_t *f = k_font[e->font < 6 ? e->font : 1];
    uint16_t c = role(e->color, T->text);
    int y = e->y + (e->flags & LG_FLOW ? s_grow : 0), al = e->flags & LG_ALIGN;

    if (!s[0])
        return;
    if (f == &font_px && !text_has_all(&font_px, s))
        f = e->flags & LG_ALT ? &font_t17 : &font_b13;
    if (e->flags & LG_WRAP) {
        text_line_t ln[2];
        int n = text_wrap(f, s, -1, e->w, 2, ln);

        if (n == 2) {
            s_grow = e->b;
            for (int i = 0; i < 2; i++) {
                int len = ln[i].len < (int)sizeof line - 1 ? ln[i].len : (int)sizeof line - 1, yb = y - e->a + text_line_h(f) * i;

                memcpy(line, s + ln[i].off, (size_t)len);
                line[len] = 0;
                ui_text(f, e->x, yb, e->w, al, line, c, yb - text_ink_up(f) - 1, yb + text_ink_down(f) + 1);
            }
            return;
        }
    }
    ui_text(f, e->x, y, e->w, al, s, c, y - text_ink_up(f) - 1, y + text_ink_down(f) + 1);
}

static void draw_cover(const lay_el_t *e)
{
    const theme_t *t = T;
    int s = e->w;

    if (g_np.cover) {
        C.x = e->x;
        C.y = e->y;
        C.s = (int16_t)s;
        C.tint = (uint8_t)(e->flags & LG_TINT ? 1 : 0);
        for (int i = 0; C.tint && i < 32; i++)
            C.pal[i] = ui_mix(t->text, t->background, i * 8 + 4);
        display_custom(e->x, e->y, e->x + s - 1, e->y + s - 1, cover_band, 0, 1);
    } else {
        uint16_t fg = 0xFFFF;

        if (e->flags & LG_SOLID) {
            ui_fill(e->x, e->y, s, s, role(e->color, t->text));
            fg = t->background;
        } else {
            uint16_t h = k_hues[g_np.hue & 7], dark = ui_mix(h, 0, 90);

            for (int k = 0; k < 4; k++)
                ui_fill(e->x, e->y + k * (s / 4), s, k == 3 ? s - 3 * (s / 4) : s / 4, ui_mix(h, dark, k * 85));
        }
        if (g_np.init[0]) {
            int w = text_width(&font_d28, g_np.init, -1);

            ui_text(&font_d28, e->x + (s - w) / 2, e->y + s / 2 + 10, w, 0, g_np.init, fg, e->y, e->y + s - 1);
        }
    }
    if ((e->flags & LG_PAUSE) && g_np.paused) {
        ui_fill(e->x + s - 22, e->y + s - 22, 18, 18, ui_mix(t->background, 0, 77));
        ui_icon(ICON_PAUSE, e->x + s - 19, e->y + s - 19, 0xFFFF);
    }
}

int player_get_volume(void);              /* (player/player_audio.c: the volume in dB) */

static void draw_bar(const lay_el_t *e)
{
    const theme_t *t = T;
    int w = e->w, fill;

    if (e->field == LF_VOLUME) {
        int db = player_get_volume();

        fill = (db + 60) * w / 60;
    } else {
        fill = g_np.dur ? (int)((uint32_t)w * (g_np.pos < g_np.dur ? g_np.pos : g_np.dur) / g_np.dur) : 0;
    }
    fill = fill < 0 ? 0 : fill > w ? w : fill;
    if (e->flags & LG_ALT) {             /* outline: a frame, the fill inside it */
        ui_frame_rect(e->x, e->y, w, e->h, role(e->color2, t->text));
        if (fill > 2)
            ui_fill(e->x + 2, e->y + 2, (fill < w - 2 ? fill : w - 2) - 2, e->h - 4, role(e->color, t->bar_fill));
    } else {                             /* line: the track, its fill, the head */
        ui_fill(e->x, e->y, w, e->h, role(e->color2, t->bar_track));
        ui_fill(e->x, e->y, fill, e->h, role(e->color, t->bar_fill));
        if (e->field == LF_PROGRESS)
            ui_fill(e->x + fill - 1, e->y - 2, 2, e->h + 4, t->text);
    }
}

static void draw_badge(const lay_el_t *e)
{
    const theme_t *t = T;
    char b[24];
    const char *s = words(e, b, sizeof b);
    int x = e->x < 0 ? s_badge_x : e->x, y = e->y, w, hi = e->color2 == 0xFE && g_np.hires;
    uint16_t bg, fg;

    if (!s[0])
        return;
    w = text_width(&font_s8, s, -1) + 6;
    if (e->flags & LG_ALT) {             /* outline */
        bg = hi ? t->hires : role(e->color, t->badge_bg);
        fg = hi ? t->hires : t->text_dim;
        ui_frame_rect(x, y, w, 11, bg);
    } else {
        bg = hi ? t->hires : role(e->color, t->badge_bg);
        fg = hi ? 0 : t->badge_text;
        ui_fill(x + 1, y, w - 2, 11, bg);
        ui_fill(x, y + 1, 1, 9, bg);
        ui_fill(x + w - 1, y + 1, 1, 9, bg);
    }
    ui_text(&font_s8, x + 3, y + 9, w - 6, 0, s, fg, y, y + 10);
    s_badge_x = x + w + 4;
}

/* mode 0: the whole cover view; 1: but the band's rows (the volume or seek band has them); 2: the band's
 * rows alone, on what is there (a view's band: ui/screens.c np_draw) */
int lay_np(int mode)
{
    int starting = g_np.seq == 0xFFFFFFFFu;

    lay_load();
    s_grow = 0;
    s_badge_x = 0;
    if (mode < 2)
        ui_fill(0, BODY_Y, DISPLAY_W, (mode ? BAND_Y : DISPLAY_H) - BODY_Y, T->background);
    for (int i = 0; i < g_lay_n; i++) {
        const lay_el_t *e = &g_lay[i];

        if (mode && (mode == 2) != (e->y >= BAND_Y))
            continue;
        if (starting && e->kind != LK_BOX && e->kind != LK_FRAME && e->kind != LK_COVER &&
            !(e->kind == LK_TEXT && e->field == LF_TITLE))
            continue;                    /* (starting: the name and the tile only) */
        switch (e->kind) {
        case LK_TEXT: draw_text(e); break;
        case LK_COVER: draw_cover(e); break;
        case LK_BOX: ui_fill(e->x, e->y, e->w, e->h, role(e->color, T->text)); break;
        case LK_FRAME: ui_frame_rect(e->x, e->y, e->w, e->h, role(e->color, T->text)); break;
        case LK_BAR: if (!starting) draw_bar(e); break;
        case LK_BADGE: draw_badge(e); break;
        }
    }
    return 1;
}

/* ---- the folder -------------------------------------------------------------------------------- */

static struct { char (*names)[LAY_NAME]; int n, max; } S;

static int found(void *u, const char *name, const fs_ref_t *ref)
{
    size_t len = strlen(name);
    int at;

    (void)u, (void)ref;
    if (fs_scan_stat()->is_dir || len < 5 || name[0] == '.' || strcmp(name + len - 4, ".txt") && strcmp(name + len - 4, ".TXT"))
        return 0;
    if (S.n >= S.max || len - 4 >= LAY_NAME)
        return 0;
    for (at = S.n; at > 0 && strcmp(S.names[at - 1], name) > 0; at--)
        memcpy(S.names[at], S.names[at - 1], LAY_NAME);
    memcpy(S.names[at], name, len - 4);
    S.names[at][len - 4] = 0;
    S.n++;
    return 0;
}

/* The Layout picker's list (ui/screens.c): names[0] "Built-in", then GILB/LAYOUTS/'s files, by name, up
 * to max in all. Returns how many | the Look's current one's place << 8. */
int lay_scan(char (*names)[LAY_NAME], int max)
{
    const char *cur = g_cfg.layout[g_cfg.look ? 1 : 0];
    int pick = 0;

    memcpy(names[0], "Built-in", 9);
    S.names = names + 1;
    S.n = 0;
    S.max = max - 1;
    (void)fs_scan(LAY_DIR, found, 0);
    for (int i = 0; i < S.n; i++)
        if (!strcmp(S.names[i], cur))
            pick = i + 1;
    g_lay_key = -1;                      /* (a file may have changed: parsed again at the next draw) */
    return (S.n + 1) | pick << 8;
}
