/*
 * frame.c -- the key bar, the header, toasts and the drawing helpers (frame.h). Geometry and
 * colours are DESIGN.md 2.1 and tools/ui_mockups.py's header() / keybar() (project repo), with
 * the key bar on the panel's top rows.
 */
#include <stdio.h>
#include <string.h>

#include "frame.h"
#include "ui.h"
#include "services/input.h"
#include "services/config.h"
#include "ui/layout.h"
#include <stddef.h>

/* ---- drawing helpers ---------------------------------------------------------------------------- */

static void icon_fn(const display_band_t *b)
{
    const icon_t *ic = &icon_table[b->arg & 0xFFu];
    uint16_t c = (uint16_t)(b->arg >> 16);

    for (int y = b->ya; y <= b->yb; y++) {
        unsigned bits = ic->rows[y - b->y0];
        uint16_t *row = b->band + (y - b->band_y) * DISPLAY_W;

        for (int x = b->cx0; x <= b->cx1; x++)
            if (bits & (0x8000u >> (x - b->x0)))
                row[x] = c;
    }
}

void ui_icon(int id, int x, int y, uint16_t c)
{
    if (id >= 0 && id < ICON_COUNT)
        display_custom(x, y, x + icon_table[id].w - 1, y + ICON_H - 1, icon_fn, (uint32_t)id | (uint32_t)c << 16, 0);
}

int ui_text(const font_t *f, int x, int base, int max_w, int align, const char *s, uint16_t c, int y0, int y1)
{
    char buf[200];
    int n, w, xa, a, b;

    if (!s || !*s || max_w <= 0)
        return 0;
    n = text_fit(f, s, -1, max_w, buf, sizeof buf);
    if (n <= 0)
        return 0;
    w = text_width(f, buf, n);
    xa = align == 1 ? x + (max_w - w) / 2 : align == 2 ? x + max_w - w : x;
    a = xa - 1 > x ? xa - 1 : x;                         /* the box: the run, 1 px each side, in the field */
    b = xa + w < x + max_w - 1 ? xa + w : x + max_w - 1;
    display_text(f, a, y0, b, y1, xa, base, buf, n, c);
    return w;
}

void ui_fill(int x, int y, int w, int h, uint16_t c)
{
    if (w > 0 && h > 0)
        display_fill_rect(x, y, x + w - 1, y + h - 1, c);
}

void ui_frame_rect(int x, int y, int w, int h, uint16_t c)
{
    ui_fill(x, y, w, 1, c);
    ui_fill(x, y + h - 1, w, 1, c);
    ui_fill(x, y + 1, 1, h - 2, c);
    ui_fill(x + w - 1, y + 1, 1, h - 2, c);
}

uint16_t ui_mix(uint16_t a, uint16_t b, int t)
{
    int r = ((a >> 11) * (256 - t) + (b >> 11) * t) >> 8;
    int g = (((a >> 5) & 63) * (256 - t) + ((b >> 5) & 63) * t) >> 8;
    int bl = ((a & 31) * (256 - t) + (b & 31) * t) >> 8;

    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* ---- the keys, the hold line, toasts, the shortcut layer ---------------------------------------- */

static keybar_t s_kb;                     /* the screen's labels: which keys have a hold */
static const char *s_layer;               /* the shortcut layer's text, NULL: off */
static int s_pressed = KEY_NONE, s_prog;
static struct { uint8_t icon, tone, on; uint32_t until; char text[64]; } s_toast;

int key_slot(int key)
{
    switch (key) {
    case KEY_UP:     return SLOT_VOLP;
    case KEY_DOWN:   return SLOT_VOLM;
    case KEY_SELECT: return SLOT_PLAY;
    case KEY_MENU:   return SLOT_MENU;
    case KEY_BACK:   return SLOT_BACK;
    default:         return -1;
    }
}

static void inval_line(void)              /* the header's text line (a toast, the layer) */
{
    ui_invalidate(0, 0, DISPLAY_W - 1, HOLD_Y - 1);
}

static void inval_hold(void)
{
    ui_invalidate(0, HOLD_Y, DISPLAY_W - 1, HOLD_Y + 1);
}

void frame_keys(const keybar_t *k)
{
    s_kb = *k;                            /* (not drawn: DESIGN 2.1 rev. 2) */
}

static int has_hold(int key)
{
    int slot = key_slot(key);

    return slot >= 0 && s_kb.hold[slot] && *s_kb.hold[slot];
}

void frame_press(int key)
{
    if (key == s_pressed)
        return;
    if (s_prog)
        inval_hold();                     /* (the line of the key before goes) */
    s_pressed = key_slot(key) >= 0 ? key : KEY_NONE;
    s_prog = 0;
}

int frame_pressed(void)
{
    return s_pressed;
}

void frame_layer(const char *text)
{
    if (text == s_layer)
        return;
    s_layer = text;
    inval_line();
}

void frame_toast(int icon, int tone, const char *text)
{
    s_toast.icon = (uint8_t)icon;
    s_toast.tone = (uint8_t)tone;
    snprintf(s_toast.text, sizeof s_toast.text, "%s", text);
    s_toast.until = time_ms() + 1500u;
    s_toast.on = 1;
    inval_line();
}

void frame_tick(uint32_t now)
{
    if (s_toast.on && (int32_t)(now - s_toast.until) >= 0) {
        s_toast.on = 0;
        inval_line();
    }
    if (s_pressed != KEY_NONE && has_hold(s_pressed)) {
        int p = input_hold_progress(s_pressed);

        if (p != s_prog) {                /* the hold line: 32 px more every 50 ms */
            s_prog = p;
            inval_hold();
        }
    }
}

/* The header line taken over (a toast, the layer): 1 when drawn. */
static int draw_overlay(void)
{
    const theme_t *t = T;

    if (s_toast.on) {
        uint16_t c = s_toast.tone == TOAST_WARN ? t->warn : s_toast.tone == TOAST_DANGER ? t->danger : t->text;
        int tx = 5;

        ui_fill(0, 0, DISPLAY_W, HOLD_Y, t->header_bg);
        if (s_toast.icon != NO_ICON) {
            ui_icon(s_toast.icon, 5, 4, s_toast.tone == TOAST_TEXT ? t->accent : c);
            tx = 22;
        }
        ui_text(&font_b13, tx, 14, DISPLAY_W - tx - 8, 0, s_toast.text, c, 0, HOLD_Y - 1);
        return 1;
    }
    if (s_layer) {
        ui_fill(0, 0, DISPLAY_W, HOLD_Y, t->sel_bg);
        ui_text(&font_s8, 5, 13, DISPLAY_W - 10, 1, s_layer, t->sel_text, 0, HOLD_Y - 1);
        return 1;
    }
    return 0;
}

static void draw_hold(void)
{
    const theme_t *t = T;

    ui_fill(0, HOLD_Y, DISPLAY_W, 1, t->header_bg);
    ui_fill(0, HOLD_Y + 1, DISPLAY_W, 1, t->divider);
    if (s_prog && s_pressed != KEY_NONE && has_hold(s_pressed))
        ui_fill(0, HOLD_Y, DISPLAY_W * s_prog / 10, 2, t->key_press);
}

/* ---- the header --------------------------------------------------------------------------------- */

enum { FD_TITLE, FD_PLAY, FD_LOCK, FD_OUT, FD_VOL, FD_BATT, FD_CLOCK, NFD };
typedef struct { int16_t x0, x1; uint32_t sig; } fld_t;

static hdr_t s_h;

static uint32_t hs(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static void battery(int x, int y, int pct, int charging)
{
    const theme_t *t = T;
    uint16_t c = pct <= 10 && !charging ? t->danger : t->text_dim;
    int w = (13 * pct + 50) / 100;

    ui_frame_rect(x, y, 17, 9, c);
    ui_fill(x + 17, y + 3, 2, 3, c);
    ui_fill(x + 2, y + 2, w < 1 ? 1 : w, 5, charging ? t->success : c);
}

#define HB 14                              /* the header's text baseline */
#define HY0 HDR_Y
#define HY1 (HOLD_Y - 1)

/* Look: Retro (services/config.h): its pixel font for the header's text, the line's own font when
 * it holds what PX lacks (a "·", accents); on Now Playing the play state and the volume at the left
 * (the position is in Retro's body: ui/retro.c). */
const font_t *look_font(const char *s, const font_t *modern)
{
    if (!g_cfg.look)
        return modern;
    return text_has_all(&font_px, s) ? &font_px : modern;
}

/* The header from the Look's layout ([header], docs/LAYOUTS.md: its parts packed from the right in their
 * lines' order, each with its font, colour, baseline and the gap before it; the title in what is left),
 * or k_hdr (Modern's) until one is parsed. Each part's span and signature, and drawn when draw. */
static const lay_el_t k_hdr[] = {
    { LK_PART, HP_BATTERY, LFN_B13, 0, 0, 14, 0, 0, 0xFF, 0xFF, 0, 0 },
    { LK_PART, HP_PERCENT, LFN_S8, 0, 0, 13, 0, 0, 0xFF, 0xFF, 3, 0 },
    { LK_PART, HP_CLOCK, LFN_S8, 0, 0, 13, 0, 0, 0xFF, 0xFF, 8, 0 },
    { LK_PART, HP_VOLUME, LFN_B13, 0, 0, 14, 0, 0, (uint8_t)(offsetof(theme_t, text) / 2u), 0xFF, 8, 0 },
    { LK_PART, HP_OUTPUT, LFN_B13, 0, 0, 14, 0, 0, 0xFF, 0xFF, 13, 0 },
    { LK_PART, HP_LOCK, LFN_B13, 0, 0, 14, 0, 0, (uint8_t)(offsetof(theme_t, warn) / 2u), 0xFF, 15, 0 },
    { LK_PART, HP_PLAY, LFN_B13, 0, 0, 14, 0, 0, 0xFF, 0xFF, 8, 0 },
    { LK_TITLE, 0, LFN_M13, 0, 0, 14, 0, 0, 0xFF, 0xFF, 0, 0 },
};

static void hdr_pass(const hdr_t *h, fld_t *f, int draw)
{
    const theme_t *t = T;
    const lay_el_t *els = g_lhdr_n ? g_lhdr : k_hdr, *ti = &k_hdr[7];
    int n = g_lhdr_n ? g_lhdr_n : (int)(sizeof k_hdr / sizeof k_hdr[0]), x = 312, w, tx, rnp;
    char pct[8];

    memset(f, 0, NFD * sizeof *f);
    for (int i = 0; i < NFD; i++)
        f[i].x0 = 1, f[i].x1 = 0;          /* (empty) */
    for (int i = 0; i < n; i++)
        if (els[i].kind == LK_TITLE)
            ti = &els[i];
    rnp = (ti->flags & LG_ALT) && h->np;  /* (np=volume: the play state and the volume as Now Playing's title) */
    snprintf(pct, sizeof pct, "%u%%", h->batt);
    for (int i = 0; i < n; i++) {
        const lay_el_t *e = &els[i];
        const char *s = e->field == HP_PERCENT ? pct : e->field == HP_CLOCK ? h->clock : e->field == HP_VOLUME ? h->vol :
                        e->field == HP_PLAY ? h->elapsed : "";
        const struct font *fn = lay_font(e->font, e->b, s);
        uint16_t c = lay_color(e->color, t->text_dim);
        fld_t *d;

        if (e->kind != LK_PART || (e->field == HP_CLOCK && !h->clock[0]) || (e->field == HP_VOLUME && rnp) ||
            (e->field == HP_LOCK && !h->lock) || (e->field == HP_PLAY && (h->np || !h->play)))
            continue;
        w = s[0] ? text_width(fn, s, -1) : 0;
        switch (e->field) {
        case HP_BATTERY:
            x -= 19;
            d = &f[FD_BATT];
            d->x0 = (int16_t)x;
            d->x1 = 312;
            d->sig = h->batt | (uint32_t)h->charging << 8 | 1u << 16;
            if (draw)
                battery(x, 5, h->batt, h->charging);
            break;
        case HP_OUTPUT:
        case HP_LOCK:
            x -= e->a;
            d = &f[e->field == HP_LOCK ? FD_LOCK : FD_OUT];
            d->x0 = (int16_t)x;
            d->x1 = (int16_t)(x + 11);
            d->sig = e->field == HP_LOCK ? 1u : h->out | 1u << 8;
            if (draw)
                ui_icon(e->field == HP_LOCK ? ICON_LOCK : h->out, x, 4, e->field == HP_LOCK ? c :
                        h->out != ICON_SPEAKER ? lay_color(e->color2, t->accent) : c);
            break;
        default:                           /* the words: the percentage, the clock, the volume, the elapsed time */
            x -= w + e->a;
            d = &f[e->field == HP_PERCENT ? FD_BATT : e->field == HP_CLOCK ? FD_CLOCK : e->field == HP_VOLUME ? FD_VOL : FD_PLAY];
            if (e->field != HP_PERCENT || d->x1 < d->x0)
                d->x1 = (int16_t)(x + w - 1);
            d->x0 = (int16_t)x;
            d->sig ^= hs(e->field == HP_PERCENT ? h->batt : 2166136261u, s);
            if (draw)
                ui_text(fn, x, e->y, w, 0, s, e->field == HP_VOLUME ? lay_color(e->color, t->text) : c, HY0, HY1);
            if (e->field == HP_PLAY) {     /* (and its play / pause mark before it) */
                x -= 13;
                d->x0 = (int16_t)x;
                d->sig = hs(h->play, h->elapsed);
                if (draw)
                    ui_icon(h->play == 1 ? ICON_PLAY : ICON_PAUSE, x, 4, c);
            }
            break;
        }
    }
    x -= 6;                                /* the title's right edge */
    f[FD_TITLE].x0 = 0;
    f[FD_TITLE].x1 = (int16_t)x;
    f[FD_TITLE].sig = hs(h->icon | (uint32_t)h->np << 8 | (uint32_t)h->mode_icon << 16 | (uint32_t)x << 24, h->title);
    if (rnp)
        f[FD_TITLE].sig = hs(f[FD_TITLE].sig ^ h->play, h->vol);
    if (!draw)
        return;
    tx = 5;
    if (rnp) {                             /* "> -3dB", as an old LCD player's */
        char v[20], *o = v;

        if (h->play)
            ui_icon(h->play == 1 ? ICON_PLAY : ICON_PAUSE, 5, 4, lay_color(ti->color, t->text));
        for (const char *s = h->vol; *s && o < v + sizeof v - 3; s++)
            if ((unsigned char)s[0] == 0xE2u && (unsigned char)s[1] == 0x88u && (unsigned char)s[2] == 0x92u)
                *o++ = '-', s += 2;       /* (U+2212, the minus PX lacks: a hyphen) */
            else
                *o++ = *s;
        memcpy(o, "dB", 3);
        ui_text(lay_font(ti->font, ti->b, v), 22, ti->y, x - 22, 0, v, lay_color(ti->color, t->text), HY0, HY1);
        return;
    }
    if (h->np) {
        ui_icon(ICON_NOTE, 5, 4, lay_color(ti->color2, t->text_dim));
        tx = 22 + ui_text(lay_font(ti->font, ti->b, h->title), 22, ti->y, x - 22, 0, h->title, lay_color(ti->color, t->text),
                          HY0, HY1) + 6;
        if (h->mode_icon != NO_ICON)
            ui_icon(h->mode_icon, tx, 4, t->accent);
        return;
    }
    if (h->icon != NO_ICON) {
        ui_icon(h->icon, 5, 4, lay_color(ti->color2, t->text_dim));
        tx = 22;
    }
    ui_text(lay_font(ti->font, ti->b, h->title), tx, ti->y, x - tx, 0, h->title, lay_color(ti->color, t->text), HY0, HY1);
}

void frame_status(const hdr_t *h)
{
    fld_t a[NFD], b[NFD];

    if (!memcmp(h, &s_h, sizeof *h))
        return;
    hdr_pass(&s_h, a, 0);
    hdr_pass(h, b, 0);
    for (int i = 0; i < NFD; i++) {
        int x0, x1;

        if (a[i].x0 == b[i].x0 && a[i].x1 == b[i].x1 && a[i].sig == b[i].sig)
            continue;
        x0 = a[i].x0 <= a[i].x1 ? a[i].x0 : b[i].x0;
        x1 = a[i].x0 <= a[i].x1 ? a[i].x1 : b[i].x1;
        if (b[i].x0 <= b[i].x1) {
            x0 = b[i].x0 < x0 ? b[i].x0 : x0;
            x1 = b[i].x1 > x1 ? b[i].x1 : x1;
        }
        if (x0 <= x1)
            ui_invalidate(x0 - 1, HY0, x1 + 1, HY1);
    }
    s_h = *h;
}

void frame_draw(void)
{
    const theme_t *t = T;
    fld_t f[NFD];

    draw_hold();
    if (draw_overlay())
        return;
    ui_fill(0, HDR_Y, DISPLAY_W, HOLD_Y, t->header_bg);
    hdr_pass(&s_h, f, 1);
}

void frame_invalidate(void)
{
    ui_invalidate(0, 0, DISPLAY_W - 1, BODY_Y - 1);
}
