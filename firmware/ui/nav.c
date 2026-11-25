/*
 * nav.c -- the page stack, the key dispatch with the global keys (DESIGN.md 3.1) and the list
 * template (2.2 L, the question's choices).
 *
 * Every pass the top page's model is compared with what is on the panel and only what changed is
 * invalidated (a list row, a key-bar slot, a header field); ui_render() then rebuilds the display
 * list from the model and pushes just those rectangles, so a partial repaint always equals a full
 * one (tests/cfw/m31_ui.sh checks it after every key).
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "nav.h"
#include "ui.h"
#include "np_extra.h"
#include "drivers/display.h"
#include "drivers/text.h"
#include "drivers/clock.h"
#include "drivers/panel_io.h"
#include "services/config.h"
#include "services/log.h"
#include "services/power.h"
#include "services/screenshot.h"
#include "player.h"
#include "services/ovl.h"              /* OVL_TEXT: functions in overlays */
#include "ui/layout.h"

const input_event_t *g_ev;
int g_locked;
static int s_force;                       /* a key came: compare the model at once (nav_tick) */

/* ---- the stack --------------------------------------------------------------------------------- */

#define NAV_MAX 16
typedef struct { const page_t *p; int16_t cur, top, arg; } ent_t;
static ent_t s_nav[NAV_MAX];
static int s_n;

typedef struct { uint16_t id, num; uint8_t icon, flags; char t[160]; char v[48]; } rc_t;
static struct {                           /* the visible rows, fitted */
    int n, y0, nr;
    rc_t c[NROWS_MAX];
} L;

/* a list row's height: its layout's (ui/layout.h g_lrow_h), but the grid's and a question's */
#define RH (nav_top() && nav_top()->kind != PK_LIST ? ROW_H : g_lrow_h)

static ent_t *top(void) { return s_n ? &s_nav[s_n - 1] : NULL; }
const page_t *nav_top(void) { return s_n ? s_nav[s_n - 1].p : NULL; }
const page_t *nav_below(void) { return s_n > 1 ? s_nav[s_n - 2].p : NULL; }
int nav_depth(void) { return s_n; }
int nav_arg(void) { return s_n ? s_nav[s_n - 1].arg : 0; }
int nav_cursor(void) { return s_n ? s_nav[s_n - 1].cur : 0; }

static int is_list(const page_t *p) { return p && p->kind != PK_CUSTOM; }

static void entered(int first)
{
    const page_t *p = nav_top();

    if (is_list(p)) {
        L.y0 = p->kind == PK_QUESTION ? 64 : LIST_Y;
        L.nr = p->kind == PK_QUESTION || p->kind == PK_GRID ? 4 : NROWS * ROW_H / g_lrow_h;   /* (147 px of rows) */
    }
    s_force = 1;
    if (p && p->enter)
        p->enter(first);
    if (is_list(nav_top()) && nav_top() == p)
        list_reload();
    ui_invalidate_all();
    nav_keys_changed();
}

void nav_reset(const page_t *root)
{
    s_n = 0;
    nav_push(root, 0);
}

void nav_push(const page_t *p, int arg)
{
    if (s_n == NAV_MAX) {                 /* (deeper than any tree here: keep the newest levels) */
        memmove(s_nav + 1, s_nav + 2, (NAV_MAX - 2) * sizeof *s_nav);
        s_n--;
    }
    s_nav[s_n].p = p;
    s_nav[s_n].cur = s_nav[s_n].top = 0;
    s_nav[s_n].arg = (int16_t)arg;
    s_n++;
    log_printf("ui: open %s", p->name);
    entered(1);
}

void nav_pop(void)
{
    const page_t *p = nav_top();

    if (s_n <= 1)
        return;
    s_n--;
    if (p->leave)
        p->leave();
    entered(0);
}

void nav_pop_to(int depth)
{
    while (s_n > depth && s_n > 1) {
        const page_t *p = s_nav[--s_n].p;

        if (p->leave)
            p->leave();
    }
    entered(0);
}

void nav_home(void)
{
    nav_pop_to(1);
}

int nav_find(const page_t *p)
{
    for (int i = s_n - 1; i >= 0; i--)
        if (s_nav[i].p == p)
            return i;
    return -1;
}

/* ---- lists ------------------------------------------------------------------------------------- */

int list_count(void)
{
    return L.n;
}

/* Row i into cache slot s, fitted: the text before the value, the value at most 150 px. */
static int fill(int s, int i)
{
    static row_t r;                       /* (210 B: not on the stack) */
    char t[160], v[48];
    int right, vw = 0, ch;

    memset(&r, 0, sizeof r);
    r.icon = NO_ICON;
    nav_top()->row(i, &r);
    right = r.flags & R_CHEV ? 300 : 310;
    v[0] = 0;
    if (r.value[0]) {
        text_fit(&font_b13, r.value, -1, 150, v, sizeof v);
        vw = text_width(&font_b13, v, -1) + 8;
    }
    text_fit(r.flags & R_BOLD ? &font_m13 : &font_b13, r.text, -1, right - vw - 24, t, sizeof t);
    ch = L.c[s].num != r.num || L.c[s].icon != r.icon || L.c[s].flags != r.flags || strcmp(L.c[s].t, t) ||
         strcmp(L.c[s].v, v);
    L.c[s].id = r.id;
    L.c[s].num = r.num;
    L.c[s].icon = r.icon;
    L.c[s].flags = r.flags;
    memcpy(L.c[s].t, t, sizeof t);
    memcpy(L.c[s].v, v, sizeof v);
    return ch;
}

static void inval_slot(int s)
{
    if (s >= 0 && s < L.nr && nav_top() && nav_top()->kind == PK_GRID)
        ui_invalidate((s & 1) * 160, LIST_Y + (s >> 1) * 73, (s & 1) * 160 + 159, LIST_Y + (s >> 1) * 73 + 72);
    else if (s >= 0 && s < L.nr)
        ui_invalidate(0, L.y0 + s * RH, 315, L.y0 + s * RH + RH - 1);
}

static void inval_list(void)
{
    ui_invalidate(0, L.y0, DISPLAY_W - 1, L.y0 + (nav_top() && nav_top()->kind == PK_GRID ? 2 * 73 : L.nr * RH) - 1);
}

static void clamp_view(ent_t *e)
{
    if (e->cur >= L.n)
        e->cur = (int16_t)(L.n ? L.n - 1 : 0);
    if (e->cur < 0)
        e->cur = 0;
    if (e->top > e->cur)
        e->top = e->cur;
    if (e->cur >= e->top + L.nr)
        e->top = (int16_t)(e->cur - L.nr + 1);
    if (e->top > L.n - L.nr)
        e->top = (int16_t)(L.n - L.nr > 0 ? L.n - L.nr : 0);
    if (nav_top() && nav_top()->kind == PK_GRID) {   /* (a row of two at a time) */
        e->top &= ~1;
        if (e->cur >= e->top + 4)
            e->top += 2;
    }
}

void list_reload(void)
{
    ent_t *e = top();
    const page_t *p = nav_top();

    if (!is_list(p))
        return;
    L.n = p->count ? p->count() : 0;
    clamp_view(e);
    for (int s = 0; s < L.nr && e->top + s < L.n; s++)
        fill(s, e->top + s);
    inval_list();
    nav_keys_changed();
}

void list_row_changed(int i)
{
    ent_t *e = top();

    if (is_list(nav_top()) && i >= e->top && i < e->top + L.nr && i < L.n && fill(i - e->top, i))
        inval_slot(i - e->top);
}

void nav_set_cursor(int i)
{
    ent_t *e = top();

    if (!is_list(nav_top())) {
        if (e)
            e->cur = (int16_t)i;
        return;
    }
    L.n = nav_top()->count ? nav_top()->count() : 0;
    e->cur = (int16_t)i;
    if (i < e->top || i >= e->top + L.nr)
        e->top = (int16_t)(i - 1 > 0 ? i - 1 : 0);
    list_reload();
}

void list_move(int d, int wrap)
{
    ent_t *e = top();
    int c, old = e->cur, otop = e->top;

    if (L.n <= 0)
        return;
    c = e->cur + d;
    if (c < 0)
        c = wrap && e->cur == 0 ? L.n - 1 : 0;
    else if (c >= L.n)
        c = wrap && e->cur == L.n - 1 ? 0 : L.n - 1;
    if (c == old)
        return;
    e->cur = (int16_t)c;
    clamp_view(e);
    if (e->top != otop) {
        for (int s = 0; s < L.nr && e->top + s < L.n; s++)
            fill(s, e->top + s);
        inval_list();
    } else {
        inval_slot(old - e->top);
        inval_slot(c - e->top);
    }
    nav_keys_changed();
}

/* ---- the rows, from the Look's layout (ui/layout.h g_lrow, docs/LAYOUTS.md [list]) ------------------
 * The layout says where a row's parts go: the cursor's highlight (select), its mark (the section icon,
 * the playing speaker, the radio, the track number), the value at the right, the label before it, the
 * chevron, boxes and frames, the scrollbar; the lay overlay parses it, this draws it (every mode shows
 * lists). Until a layout is parsed (or when it cannot be): Modern's row, k_row. */
static const lay_el_t k_row[] = {
    { LK_SELECT, 0, 0, 0, 0, 0, 316, 21, 0xFF, 0xFF, 0, 0 },
    { LK_MARK, 0, LFN_S8, 0, 6, 5, 2, 18, 0xFF, 0xFF, 15, 0 },
    { LK_VALUE, 0, LFN_B13, 0, 24, 15, 286, 0, 0xFF, 0xFF, 8, 0 },
    { LK_LABEL, 0, LFN_B13, 0, 24, 15, 286, 0, 0xFF, 0xFF, LFN_M13 + 1, 0 },
    { LK_CHEV, 0, 0, 0, 304, 5, 0, 0, 0xFF, 0xFF, 10, 0 },
    { LK_SCROLL, 0, 0, 0, 316, 0, 0, 0, 0xFF, 0xFF, 0, 0 },
};

static const lay_el_t *row_els(int *n)
{
    if (!g_lrow_n) {
        *n = (int)(sizeof k_row / sizeof k_row[0]);
        return k_row;
    }
    *n = g_lrow_n;
    return g_lrow;
}

static const font_t *const k_lfont[] = { &font_s8, &font_b13, &font_m13, &font_t17, &font_d28, &font_px };

const struct font *lay_font(unsigned f, unsigned alt, const char *s)
{
    const font_t *ft = k_lfont[f < 6u ? f : 1u];

    if (ft == &font_px && !text_has_all(&font_px, s))
        return k_lfont[alt && alt <= 6u ? alt - 1u : 1u];
    return ft;
}

/* ---- the other screens, from their sections (phase 3: [question], [slider], [info]) ---------------
 * The lay overlay parses the one shown into g_lscr (ap_lay_screen); this draws it. When it cannot be
 * parsed (USB-C earphones hold the overlay's window) and g_lscr has another screen's: Modern's, below. */
#define TO(f) (uint8_t)(offsetof(theme_t, f) / 2u)
static const lay_el_t k_lq[] = {
    { LK_ICON, 0, LFN_B13, 0, 10, 28, 0, 0, LCOL_DEF, LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_LINE1, LFN_M13, 0, 28, 38, 272, 0, LCOL_TONE, LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_LINE2, LFN_B13, 0, 28, 56, 272, 0, TO(text_dim), LCOL_DEF, 0, 0 },
    { LK_BOX, 0, LFN_B13, 0, 0, 63, 320, 1, TO(divider), LCOL_DEF, 0, 0 },
};
static const lay_el_t k_lsl[] = {
    { LK_TEXT, LF_LABEL, LFN_B13, 0, 16, 44, 288, 0, TO(text_dim), LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_VALUE, LFN_D28, 1, 0, 84, 320, 0, LCOL_DEF, LCOL_DEF, 0, 0 },
    { LK_BAR, LF_LEVEL, LFN_B13, LG_TICKS, 16, 100, 288, 8, LCOL_DEF, LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_MIN, LFN_S8, 0, 16, 124, 40, 0, TO(text_dim), LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_MAX, LFN_S8, 2, 200, 124, 104, 0, TO(text_dim), LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_NOTE, LFN_B13, 0, 16, 150, 288, 0, TO(text_dim), LCOL_DEF, 0, 0 },
};
static const lay_el_t k_linfo[] = {
    { LK_TEXT, LF_LABEL, LFN_B13, 0, 10, 36, 118, 0, TO(text_dim), LCOL_DEF, 0, 0 },
    { LK_TEXT, LF_VALUE, LFN_B13, 0, 130, 36, 182, 0, LCOL_TONE, LCOL_DEF, 0, 0 },
};

const lay_el_t *lay_screen_els(int id, int *n)
{
    if ((g_lscr_id != id || g_lscr_key != g_lay_key || g_lay_key < 0) && !ap_lay_screen(id) && g_lscr_id != id) {
        g_lscr_row = 18;                  /* (g_lscr is another screen's: Modern's built-in) */
        *n = id == LS_QUESTION ? 4 : id == LS_SLIDER ? 6 : 2;
        return id == LS_QUESTION ? k_lq : id == LS_SLIDER ? k_lsl : k_linfo;
    }
    *n = g_lscr_n;
    return g_lscr;
}

void lay_screen_draw(int id, const lay_ctx_t *c, int dy)
{
    const theme_t *t = T;
    int n;
    const lay_el_t *els = lay_screen_els(id, &n);
    char b[72];

    for (int i = 0; i < n; i++) {
        const lay_el_t *e = &els[i];
        int y = e->y + dy, w = e->w;
        uint16_t col = e->color == LCOL_TONE ? c->tone : lay_color(e->color, e->kind == LK_BOX || e->kind == LK_FRAME ?
                                                                              t->divider : t->text);

        switch (e->kind) {
        case LK_BOX:
            ui_fill(e->x, y, w, e->h, col);
            break;
        case LK_FRAME:
            ui_frame_rect(e->x, y, w, e->h, col);
            break;
        case LK_ICON:
            if (c->icon != NO_ICON)
                ui_icon(c->icon, e->x, y, e->color == LCOL_DEF ? c->icon_c : col);
            break;
        case LK_BAR: {                    /* the level v of lo..hi: a box a step, outlined, or a line */
            int span = c->hi > c->lo ? c->hi - c->lo : 1, k = c->v - c->lo, f = w * k / span;
            uint16_t fc = e->color == LCOL_DEF ? t->bar_fill : col;

            if (e->flags & LG_STEPS) {
                int sw = w / (span + 1);

                for (int j = 0; j <= span; j++)
                    (j <= k ? ui_fill : ui_frame_rect)(e->x + sw * j, y, sw - 4, e->h, fc);
                break;
            }
            if (e->flags & LG_ALT) {
                ui_frame_rect(e->x, y, w, e->h, lay_color(e->color2, t->text));
                if (f > 2)
                    ui_fill(e->x + 2, y + 2, (f < w - 2 ? f : w - 2) - 2, e->h - 4, fc);
            } else {
                ui_fill(e->x, y, w, e->h, lay_color(e->color2, t->bar_track));
                ui_fill(e->x, y, f, e->h, fc);
                ui_fill(e->x + f - 2, y - 4, 4, e->h + 8, t->text);
            }
            for (int j = 0; (e->flags & LG_TICKS) && j <= span; j++)
                ui_fill(e->x + w * j / span, y + e->h + 2, 1, 3, t->text_dim);
            break;
        }
        case LK_TEXT: {                   /* px gives way to alt for what it lacks, and when it is too wide */
            const char *s = c->words(e->field, b, sizeof b);
            const font_t *f = lay_font(e->font, e->b, s);

            if (!s[0])
                break;
            if (f == &font_px && e->b && text_width(f, s, -1) > w)
                f = k_lfont[e->b - 1u < 6u ? e->b - 1u : 1u];
            ui_text(f, e->x, y, w, e->flags & LG_ALIGN, s, col, y - text_ink_up(f) - 1, y + text_ink_down(f) + 1);
            break;
        }
        }
    }
}

/* An element's font for s: px falls back to b13 (one row higher) for what it lacks. */
static const font_t *row_font(unsigned f, const char *s, int *y)
{
    const font_t *ft = k_lfont[f < 6u ? f : 1u];

    if (ft == &font_px && !text_has_all(&font_px, s)) {
        (*y)--;
        return &font_b13;
    }
    return ft;
}

static void draw_row(int s, int y, int sel)
{
    const theme_t *t = T;
    const rc_t *c = &L.c[s];
    int n, off = c->flags & R_OFF, take = 0, vleft = 0x7FFF, mark_r = -1;
    const lay_el_t *els = row_els(&n), *val = NULL;

    for (int i = 0; i < n; i++)          /* the chevron's room and the value's place, for the label */
        if (els[i].kind == LK_CHEV && (c->flags & R_CHEV))
            take = els[i].a;
        else if (els[i].kind == LK_VALUE)
            val = &els[i];
    if (val && c->v[0]) {
        int yv = y + val->y, w = text_width(row_font(val->font, c->v, &yv), c->v, -1);

        w = val->b && w > val->b ? val->b : w;
        vleft = val->x + val->w - take - w - val->a;
    }
    for (int i = 0; i < n; i++) {
        const lay_el_t *e = &els[i];
        int x = e->x, yb = y + e->y;
        const font_t *f;

        switch (e->kind) {
        case LK_SELECT:
            if (sel)
                ui_fill(x, y + e->y, e->w, e->h, lay_color(e->color, t->sel_bg));
            break;
        case LK_BOX:
            ui_fill(x, y + e->y, e->w, e->h, lay_color(e->color, t->divider));
            break;
        case LK_FRAME:
            ui_frame_rect(x, y + e->y, e->w, e->h, lay_color(e->color, t->divider));
            break;
        case LK_MARK: {
            uint16_t base = sel ? t->sel_text : off && e->color == LCOL_DEF ? t->text_off : lay_color(e->color, t->text_dim),
                     on = sel ? t->sel_text : lay_color(e->color2, t->accent);

            if (c->flags & R_PLAYING) {
                ui_icon(ICON_SPEAKER, x, yb, on);
                mark_r = x + 12;
            } else if (c->flags & R_RADIO) {
                ui_icon(c->flags & R_ON ? ICON_RADIO_ON : ICON_RADIO_OFF, x, yb, c->flags & R_ON ? on : base);
                mark_r = x + 12;
            } else if (c->flags & R_NUM) {
                char nb[8];
                int ny = y + e->a;

                snprintf(nb, sizeof nb, "%u", c->num);
                f = row_font(e->font, nb, &ny);
                ui_text(f, e->w, ny, e->h, 2, nb, sel ? t->sel_text : lay_color(e->color, t->text_dim), y, y + RH - 1);
                mark_r = e->w + e->h;
            } else if (c->icon != NO_ICON && !(e->flags & LG_SOLID)) {
                ui_icon(c->icon, x, yb, base);
            }
            break;
        }
        case LK_VALUE:
            if (c->v[0]) {
                int w;

                f = row_font(e->font, c->v, &yb);
                w = vleft + e->a;
                ui_text(f, w, yb, e->x + e->w - take - w, 0, c->v,
                        sel ? t->sel_text : off ? t->text_off : lay_color(e->color, t->value), y, y + RH - 1);
            }
            break;
        case LK_LABEL: {
            int r = e->x + e->w - take;

            if (e->b && mark_r >= 0 && x < mark_r + e->b)
                x = mark_r + e->b;
            if (vleft < r)
                r = vleft;
            f = (c->flags & R_BOLD) && e->a ? k_lfont[e->a - 1u < 6u ? e->a - 1u : 1u] : row_font(e->font, c->t, &yb);
            ui_text(f, x, yb, r - x, 0, c->t, sel ? t->sel_text : off ? t->text_off : lay_color(e->color, t->text),
                    y, y + RH - 1);
            break;
        }
        case LK_CHEV:
            if (c->flags & R_CHEV) {
                uint16_t cc = sel ? t->sel_text : lay_color(e->color, t->text_dim);

                if (e->flags & LG_ALT)
                    ui_text(k_lfont[e->font < 6u ? e->font : 1u], x, yb, 9, 0, ">", cc, y, y + RH - 1);
                else
                    ui_icon(ICON_CHEV, x, yb, cc);
            }
            break;
        }
    }
}

int ap_grid_draw(void);                   /* startup/ap_ovl.c: nav_grid_draw in the viz overlay */

/* the scrollbar: line (a 1 px track, a 3 px thumb) or outline (a frame, a 2 px thumb in it), at x */
static void scrollbar(const lay_el_t *e, int h)
{
    const theme_t *t = T;
    const ent_t *en = top();
    int o = e->flags & LG_ALT, in = o ? 6 : 4, th = (h - in) * L.nr / L.n, y;

    th = th < 8 ? 8 : th;
    y = L.y0 + in / 2 + (h - in - th) * en->top / (L.n - L.nr);
    if (o) {
        ui_frame_rect(e->x, L.y0 + 1, 6, h - 2, lay_color(e->color2, t->text));
        ui_fill(e->x + 2, y, 2, th, lay_color(e->color, t->text));
    } else {
        ui_fill(e->x + 1, L.y0 + 2, 1, h - 4, lay_color(e->color2, t->divider));
        ui_fill(e->x, y, 3, th, lay_color(e->color, t->scroll));
    }
}

static void list_draw(void)
{
    const theme_t *t = T;
    const ent_t *e = top();
    int h = L.nr * RH, n;
    const lay_el_t *els;

    if (g_lay_key < 0 && ap_lay_load() && nav_top()->kind == PK_LIST && L.nr != NROWS * ROW_H / g_lrow_h) {
        L.nr = NROWS * ROW_H / g_lrow_h;  /* (a layout with another row height: the rows refitted, drawn again) */
        list_reload();
        ui_invalidate_all();
        return;
    }
    if (nav_top()->kind == PK_GRID && ap_grid_draw())
        return;                           /* (else as a list of four; the layout parsed first: its overlay is
                                             the grid's window, which must be the last to load) */
    ui_fill(0, L.y0, DISPLAY_W, h, t->background);
    if (!L.n) {
        ui_text(g_cfg.look ? &font_px : &font_b13, 0, L.y0 + 50, DISPLAY_W, 1, "Nothing here",
                g_cfg.look ? t->text : t->text_dim, L.y0, L.y0 + h - 1);
        return;
    }
    for (int s = 0; s < L.nr && e->top + s < L.n; s++)
        draw_row(s, L.y0 + s * RH, e->top + s == e->cur);
    els = row_els(&n);
    for (int i = 0; L.n > L.nr && i < n; i++)
        if (els[i].kind == LK_SCROLL)
            scrollbar(&els[i], h);
}

/* ---- PK_GRID: a list as a 2 x 2 grid of covers (Library > Albums) -----------------------------------
 * The list's own model (cursor, rows, scrolling), four rows visible, a row of two at a time; drawn by
 * nav_grid_draw() in the viz overlay (firmware_ap.ld, by section) through ap_grid_draw(). Each album's
 * cover56.rgb (56 x 56 RGB565) is streamed from its file band by band while the screen is flushed:
 * no picture in RAM. g_grid_cover (ui/lib_ui.c, as it gives the rows): the visible albums' covers. */
#define GRID_CW 160
#define GRID_CH 73
#define GRID_Y0 LIST_Y

static uint8_t s_gtint;                    /* Look: Retro: the covers in the theme's two colours */
static uint16_t s_gpal[32];                /* (dark: the text .. light: the background, as ui/retro.c) */

OVL_TEXT(viz) static void grid_cover(const display_band_t *b)
{
    uint16_t rows[16 * 56];                    /* (a band's rows of one cover: 1.8 KB of stack) */
    int n = b->yb - b->ya + 1;
    fs_file_t *f = fs_open_ref(&g_grid_cover[b->arg & 3u]);
    long got = 0;

    if (f && n <= 16 && fs_seek(f, (long)(b->ya - b->y0) * 112) >= 0)
        got = fs_read(f, rows, (unsigned long)n * 112u);
    if (f)
        fs_close(f);
    for (int y = b->ya; y <= b->yb; y++) {
        uint16_t *row = b->band + (y - b->band_y) * DISPLAY_W;
        const uint16_t *src = rows + (y - b->ya) * 56;

        for (int x = b->cx0; x <= b->cx1; x++) {
            uint16_t p = got == (long)n * 112 ? src[x - b->x0] : 0x2104;

            if (s_gtint)
                p = s_gpal[((p >> 11) * 8u * 77u + ((p >> 5) & 63u) * 4u * 150u + (p & 31u) * 8u * 29u) >> 11];
            row[x] = p;
        }
    }
}

OVL_TEXT(viz) void nav_grid_draw(void)
{
    static const uint16_t hues[8] OVL_RODATA(viz) = { 0x7A7A, 0x2C70, 0xC287, 0x3B78, 0xAA2B, 0x5C65, 0xB3C3, 0x4AF5 };
    const theme_t *t = T;
    const ent_t *e = top();
    int h = L.nr ? 7 * ROW_H : 0, r = g_cfg.look;   /* r: Look: Retro (PX, tinted covers, its scrollbar) */

    s_gtint = (uint8_t)r;
    for (int i = 0; r && i < 32; i++)
        s_gpal[i] = ui_mix(t->text, t->background, i * 8 + 4);
    ui_fill(0, GRID_Y0, DISPLAY_W, 7 * ROW_H, t->background);
    if (!L.n) {
        ui_text(r ? &font_px : &font_b13, 0, GRID_Y0 + 50, DISPLAY_W, 1, "Nothing here", t->text_dim, GRID_Y0,
                GRID_Y0 + h - 1);
        return;
    }
    for (int s = 0; s < 4 && e->top + s < L.n; s++) {
        const rc_t *c = &L.c[s];
        int x = (s & 1) * GRID_CW, y = GRID_Y0 + (s >> 1) * GRID_CH, sel = e->top + s == e->cur;
        int cy0 = y, cy1 = y + GRID_CH - 1, ty;
        text_line_t ln[2];
        const font_t *fn = r && text_has_all(&font_px, c->t) ? &font_px : &font_b13, *fv = r ? &font_px : &font_b13;
        int nl = text_wrap(fn, c->t, -1, 84, 2, ln);
        char b[100];
        const fs_ref_t *cv = &g_grid_cover[(e->top + s) & 3];

        if (sel)
            ui_fill(x + 2, y + 1, GRID_CW - 4, GRID_CH - 2, t->sel_bg);
        if (cv->size == 56u * 56u * 2u && g_grid_index[(e->top + s) & 3] == e->top + s) {
            if (r && sel)                      /* (Retro: a tinted cover's frame on the cursor's cell) */
                ui_frame_rect(x + 6, y + 6, 60, 60, t->sel_text);
            display_custom(x + 8, y + 8, x + 63, y + 63, grid_cover, (uint32_t)((e->top + s) & 3), 1);
        } else {                               /* no cover: the monogram tile, as Now Playing's */
            int k = (uint8_t)c->t[0], len = (k & 0xE0) == 0xC0 ? 2 : (k & 0xF0) == 0xE0 ? 3 : 1;

            /* (Retro: one colour, inverted on the cursor's cell, which is that colour) */
            ui_fill(x + 8, y + 8, 56, 56, r ? (sel ? t->sel_text : t->text) : hues[k & 7]);
            memcpy(b, c->t, (size_t)len);
            b[c->t[0] ? len : 0] = 0;
            ui_text(r && len == 1 ? &font_px : &font_t17, x + 8, y + 42, 56, 1, b,
                    r ? (sel ? t->sel_bg : t->background) : 0xFFFF, cy0, cy1);
        }
        ty = g_cfg.grid ? y + 22 : y + 42;     /* (album_grid: year only, beside the cover's middle) */
        for (int i = 0; g_cfg.grid && i < nl; i++, ty += 16) {
            int len = ln[i].len < (int)sizeof b - 1 ? ln[i].len : (int)sizeof b - 1;

            memcpy(b, c->t + ln[i].off, (size_t)len);
            b[len] = 0;
            ui_text(fn, x + 70, ty, 84, 0, b, sel ? t->sel_text : t->text, cy0, cy1);
        }
        ui_text(fv, x + 70, ty + (g_cfg.grid ? 2 : 0), 84, 0, c->v, sel ? t->sel_text : g_cfg.grid ?
                t->text_dim : t->text, cy0, cy1);
    }
    if (L.n > 4) {                             /* the scrollbar, by rows of two */
        int rows = (L.n + 1) / 2, th = (h - 4) * 2 / rows, y;

        th = th < 8 ? 8 : th;
        y = GRID_Y0 + 2 + (h - 4 - th) * (e->top / 2) / (rows - 2 > 0 ? rows - 2 : 1);
        if (r) {                               /* (Retro's: outlined, as its lists') */
            ui_frame_rect(313, GRID_Y0 + 1, 6, h - 2, t->text);
            ui_fill(315, y, 2, th, t->text);
        } else {
            ui_fill(317, GRID_Y0 + 2, 1, h - 4, t->divider);
            ui_fill(316, y, 3, th, t->scroll);
        }
    }
}

/* ---- the frame --------------------------------------------------------------------------------- */

void nav_keys_changed(void)
{
    const page_t *p = nav_top();
    keybar_t k;

    memset(&k, 0, sizeof k);
    for (const keydef_t *d = p ? p->keys : NULL; d && d->key; d++) {
        int s = key_slot(d->key);
        const char *l = d->label;

        if (s < 0 || !l)
            continue;
        if (l == VERB)
            l = p->verb ? p->verb(nav_cursor(), d->gest == IG_HOLD) : "OPEN";
        if (d->gest == IG_HOLD)
            k.hold[s] = l;
        else if (d->gest == IG_TAP || d->gest == IG_DOWN)
            k.tap[s] = l;
    }
    frame_keys(&k);
}

void nav_draw(void)
{
    const page_t *p = nav_top();

    display_clear(T->background);
    frame_draw();
    if (!p)
        return;
    if (p->kind == PK_QUESTION && p->draw)
        p->draw();
    if (is_list(p))
        list_draw();
    else if (p->draw)
        p->draw();
}

void nav_toast(int icon, int tone, const char *fmt, ...)
{
    char b[64];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    frame_toast(icon, tone, b);
    log_printf("ui: toast %s", b);
}

/* ---- keys -------------------------------------------------------------------------------------- */

/* the shortcut layer, named in the header line (DESIGN 2.1 rev. 2) */
static const char k_layer[] = "PLAY SHOT \xC2\xB7 MENU LOCK \xC2\xB7 BACK PANEL \xC2\xB7 VOL BRIGHTNESS";
static uint32_t s_layer_at;               /* the shortcut layer: when it began, 0 = off */
static int s_shot;                        /* a screenshot after the next frame (without the layer) */
static struct { uint32_t t; const page_t *p; int d; } s_undo;   /* the last VOL press, for the chord */
static int s_menu_shown;                  /* the power menu came up for this press */

int nav_volume_keys(void)
{
    return !power_screen_on() || (nav_top() && nav_top()->kind == PK_CUSTOM && nav_top()->keys &&
                                  nav_top()->keys[0].act == A_VOL_UP);
}

static void log_key(const input_event_t *g)
{
    static const char *const k_names[] = { "none", "up", "down", "select", "back", "power", "menu", "chord" };
    const char *on = nav_top() ? nav_top()->name : "-";
    int vol = g->key == KEY_UP || g->key == KEY_DOWN, leg = 0;

    if (!(g->gesture == IG_DOWN || (g->gesture == IG_END && g->key != KEY_SELECT) ||
          (vol && (g->gesture == IG_TAP || g->gesture == IG_HOLD)))) {
        if (g->gesture == IG_REPEAT)
            log_printf("ui: key %s.repeat on %s t=%u held=%u steps=%u n=%u", input_key_name(g->key), on,
                       (unsigned)g->t_ms, (unsigned)g->held_ms, (unsigned)g->steps, (unsigned)g->repeat);
        else    /* (in: the edge to this event; rise, blips: the ladder's last release, panel_io.h) */
            log_printf("ui: key %s.%s on %s t=%u held=%u in=%u rise=%u blips=%u %x", input_key_name(g->key),
                       input_gesture_name(g), on, (unsigned)g->t_ms, (unsigned)g->held_ms, (unsigned)g->in_ms,
                       (unsigned)(keys_release_stats() & 0xFFFFu), (unsigned)(keys_release_stats() >> 16),
                       (unsigned)keys_blip_stats());   /* (blips by how long they read idle: panel_io.h) */
    }
    /* the old events' lines too (input_poll's: VOL on its press and repeats, the others on a tap or a
     * hold, the chord), which the tests count */
    if (vol && (g->gesture == IG_DOWN || g->gesture == IG_REPEAT))
        leg = g->gesture == IG_REPEAT ? 2 : 1;
    else if (!vol && (g->gesture == IG_TAP || g->gesture == IG_HOLD || g->gesture == IG_CHORD))
        leg = g->gesture == IG_HOLD ? 3 : 1;
    if (leg)
        log_printf("ui: key %s%s on %s", k_names[g->key < 8 ? g->key : 0],
                   leg == 2 ? " (repeat)" : leg == 3 ? " (hold)" : "", on);
}

int ap_power_ok(void);                    /* ap_ui.c: the power key's press began after boot's */
void ap_brightness(int d);
void ap_screen(int on);

static void layer_key(const input_event_t *ev)
{
    s_layer_at = 0;
    frame_layer(NULL);
    switch (ev->key) {
    case KEY_UP:
    case KEY_DOWN:
        ap_brightness(ev->key == KEY_UP ? 1 : -1);
        nav_toast(ICON_SUN, TOAST_TEXT, "Brightness %u / 8", g_cfg.brightness);
        break;
    case KEY_SELECT:
        s_shot = 1;                       /* after the frame without the layer */
        break;
    case KEY_MENU:
        g_locked ^= 1;
        nav_toast(ICON_LOCK, TOAST_TEXT, g_locked ? "Keys locked" : "Keys unlocked");
        break;
    case KEY_BACK:
        log_printf("ui: panel redraw");
        ui_invalidate_all();
        break;
    }
}

static const keydef_t *lookup(const page_t *p, const input_event_t *ev)
{
    for (const keydef_t *d = p ? p->keys : NULL; d && d->key; d++)
        if (d->key == ev->key && d->gest == ev->gesture)
            return d;
    return NULL;
}

void np_library(void);                    /* screens.c: the playing track's folder */
void np_band_off(void);                   /* screens.c: the progress band again */

/* Now Playing on top: back to it if it is on the stack, else pushed. 0 when nothing is loaded. */
static int go_np(void)
{
    int k = nav_find(&pg_np);

    if (k >= 0)
        nav_pop_to(k + 1);
    else if (np_loaded())
        nav_push(&pg_np, 0);
    else
        return 0;
    return 1;
}

static void toggle_np(void)
{
    if (nav_top() == &pg_np) {
        if (nav_below() && nav_below() != &pg_home)
            nav_pop();
        else
            np_library();
    } else if (!go_np()) {
        nav_toast(ICON_NOTE, TOAST_TEXT, "Nothing is playing");
    }
}

static void act(int a)
{
    const page_t *p = nav_top();

    switch (a) {
    case A_UP:
    case A_DOWN:
        if (is_list(p)) {
            int n = g_ev->gesture == IG_REPEAT ? g_ev->steps : 1;

            list_move(a == A_UP ? -n : n, g_ev->gesture == IG_DOWN);
        } else if (p->act) {
            p->act(a, nav_cursor());
        }
        break;
    case A_BACK:
    case A_CLOSE:
        nav_pop();
        break;
    case A_HOME:
        nav_home();
        break;
    case A_NP:
        toggle_np();
        break;
    case A_OPTIONS:
        nav_push(p->options ? p->options : &pg_opts, 0);
        break;
    case A_NOP:
        break;
    case A_GO_NP:                         /* a menu's PLAY hold (Home, Settings, Bluetooth) */
        if (!np_loaded() || !go_np()) {
            if (p->act)
                p->act(A_OPEN_HOLD, nav_cursor());   /* (nothing loaded: what the hold did before) */
        }
        break;
    default:
        if (p->act)
            p->act(a, nav_cursor());
        break;
    }
}

void nav_key(const input_event_t *ev)
{
    const keydef_t *d;
    int on = power_screen_on();
    unsigned seq;

    g_ev = ev;
    s_force = 1;
    log_key(ev);
    if (ev->gesture != IG_END)            /* (the multi-press window closing is no key: not activity) */
        power_activity();
    if (ev->key == KEY_POWER) {           /* tap: the screen off / on (the menu: nav_tick) */
        if (ev->gesture == IG_TAP && ap_power_ok() && !s_menu_shown)
            ap_screen(!on);
        if (ev->gesture == IG_TAP || ev->gesture == IG_RELEASE)
            s_menu_shown = 0;
        return;
    }
    if (ev->gesture == IG_CHORD) {        /* the shortcut layer; the first key's step undone */
        np_seek_cancel();
        if (ev->t_ms - s_undo.t < 600u && s_undo.p == nav_top()) {
            if (is_list(s_undo.p)) {
                list_move(s_undo.d, 1);
            } else if (s_undo.p->act) {     /* the opposite step (volume, an editor's value) */
                input_event_t u = *ev;

                u.gesture = IG_DOWN;
                g_ev = &u;
                s_undo.p->act(s_undo.d > 0 ? A_VOL_UP : A_VOL_DN, nav_cursor());
                g_ev = ev;
                np_band_off();
            }
        }
        s_undo.p = NULL;
        frame_press(KEY_NONE);
        if (on) {
            s_layer_at = time_stamp(time_ms());
            frame_layer(k_layer);
        }
        log_printf("ui: shortcut layer");
        return;
    }
    if (s_layer_at) {
        if ((ev->key == KEY_UP || ev->key == KEY_DOWN) ? ev->gesture == IG_DOWN : ev->gesture == IG_TAP)
            layer_key(ev);
        return;
    }
    if (!on) {                            /* screen off: the music keys work blind, MENU / BACK wake */
        if (ev->key == KEY_UP || ev->key == KEY_DOWN || ev->key == KEY_SELECT) {
            const page_t *mp = nav_top() == &pg_dac ? &pg_dac : &pg_np;

            d = lookup(mp, ev);
            if (d && !g_locked)
                mp->act(d->act, 0);
        } else if (ev->gesture == IG_TAP) {
            ap_screen(1);
        }
        return;
    }
    if (ev->gesture == IG_DOWN) {
        frame_press(ev->key);             /* (key->px is marked by what the key does, below: one line a key) */
    } else if (ev->gesture == IG_TAP || ev->gesture == IG_RELEASE) {
        frame_press(KEY_NONE);
    }
    if (g_locked) {
        if (ev->gesture == IG_TAP)        /* (on the release: the chord that unlocks starts with a VOL press) */
            nav_toast(ICON_LOCK, TOAST_WARN, "Keys locked: VOL+ & VOL\xE2\x88\x92, then MENU");
        return;
    }
    d = lookup(nav_top(), ev);
    if (!d || d->act == A_NONE)
        return;
    seq = ui_inval_seq;
    s_undo.p = NULL;
    act(d->act);
    if (ui_inval_seq == seq)
        return;
    /* key->px: the result (when the key changed something), from the finger's edge (a DOWN's t_ms is
     * the sample that sent it: its press was in_ms before) */
    display_mark_input(ev->t_ms - (ev->gesture == IG_DOWN ? ev->in_ms : 0u), ev->in_ms);
    if (ev->gesture == IG_DOWN && (ev->key == KEY_UP || ev->key == KEY_DOWN)) {   /* for the chord */
        s_undo.t = ev->t_ms;
        s_undo.p = nav_top();
        s_undo.d = d->act == A_UP || d->act == A_VOL_UP ? (is_list(s_undo.p) ? 1 : -1) : (is_list(s_undo.p) ? -1 : 1);
    }
}

void np_power_menu(void);                 /* screens.c */

static uint32_t s_live, s_model_at;

void nav_live_now(void)
{
    s_live = time_ms() - 1000u;
}

void nav_tick(void)
{
    uint32_t now = time_ms();
    const page_t *p = nav_top();
    hdr_t h;

    if (s_layer_at && now - s_layer_at >= 2000u) {
        s_layer_at = 0;
        frame_layer(NULL);
    }
    if (s_shot && !s_layer_at && !ui_is_dirty()) {
        int r = screenshot_save();

        s_shot = 0;
        log_printf("ui: screenshot: %d", r);
        nav_toast(ICON_CHECK, TOAST_TEXT, r >= 0 ? "Screenshot saved" : "Screenshot failed");
    }
    if (!s_menu_shown && ap_power_ok() && input_held_ms(KEY_POWER) >= 1000u) {
        s_menu_shown = 1;                 /* (held on to 2 s: power off, ap_ui.c) */
        if (!power_screen_on())
            ap_screen(1);
        np_power_menu();
    }
    frame_tick(now);
    /* The model is compared with the panel every 40 ms (and at once after a key): what changes by
     * itself (the time, the battery, a BT link) shows within 40 ms, and core A spends a pass's
     * worth of comparisons four times less often while music plays (tests/cfw/m3_audio.sh budget). */
    if (!s_force && now - s_model_at < 40u)
        return;
    s_force = 0;
    s_model_at = now;
    memset(&h, 0, sizeof h);
    nav_header(&h);
    {
        static uint8_t s_out = ICON_SPEAKER;

        if (h.out != s_out) {             /* (the header's output field: a BT link up or down) */
            s_out = h.out;
            log_printf("ui: header output %s", h.out == ICON_BT ? "bt" : h.out == ICON_USBC ? "usb-c" : "jack");
        }
    }
    frame_status(&h);
    if (p && p->tick)
        p->tick();
    p = nav_top();
    if (p && p->live && is_list(p) && now - s_live >= 250u) {
        ent_t *e = top();
        int n = p->count ? p->count() : 0;

        s_live = now;
        if (n != L.n) {                   /* rows came or went: the cursor stays on its row (by id) */
            static row_t r;
            int s = e->cur - e->top, id = s >= 0 && s < L.nr && e->cur < L.n ? L.c[s].id : -1;

            for (int i = 0; id >= 0 && i < n; i++) {
                memset(&r, 0, sizeof r);
                p->row(i, &r);
                if (r.id == id) {
                    e->cur = (int16_t)i;
                    break;
                }
            }
            list_reload();
        } else {
            for (int s = 0; s < L.nr && e->top + s < L.n; s++)
                if (fill(s, e->top + s))
                    inval_slot(s);
            nav_keys_changed();
        }
    }
    if (p && p->kind == PK_CUSTOM)
        nav_keys_changed();               /* (Now Playing's PLAY label follows the state) */
}
