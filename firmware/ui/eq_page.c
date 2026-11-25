/*
 * eq_page.c -- Settings > Playback > EQ > Custom (ui/screens.c pg_eq): ten bands, a bar each; VOL+/VOL-
 * the band's gain (-12..+12 dB, and the EQ to Custom), PLAY the next band, PLAY held the band to 0 dB.
 * Each change goes to core B at once (audio_eq: crossfaded) and is saved 5 s later.
 *
 * In the usbout overlay (firmware_ap.ld, startup/ap_ovl.c ap_eq): core A's resident
 * code is full, and USB-C earphones keep that overlay in the window while they are on, so the editor
 * never evicts them (any other overlay would, every main-loop pass).
 */
#include <stdio.h>

#include "nav.h"
#include "ui.h"
#include "audio/audio.h"
#include "services/config.h"
#include "ui/layout.h"

static uint8_t s_band;

static const char *const k_hz[10] = { "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };

static const char *eq_words(const lay_el_t *e, char *b, int cap)
{
    if (e->field != LF_BAND)
        return "";
    snprintf(b, (unsigned)cap, "%s Hz   %s%d dB", k_hz[s_band], g_cfg.eq_gain[s_band] > 0 ? "+" : "", g_cfg.eq_gain[s_band]);
    return b;
}

/* The ten bands in the layout's bar of kind=eq: a band's centre every w / 10 px, 0 dB at its middle row.
 * A line: a track and the gain's fill (12 dB: half its height); steps: an LCD's segments up or down,
 * 6 of 2 dB (the unlit ones in track=). The band edited bright (steps: its label inverted). */
static void bands(const lay_el_t *e)
{
    const theme_t *t = T;
    int p = e->w / 10, zl = e->y + e->h / 2, st = e->flags & LG_STEPS;
    uint16_t lit = lay_color(e->color, st ? t->text : t->bar_fill), off = lay_color(e->color2, st ? t->text_off : t->bar_track);
    const font_t *f = lay_font(e->font, 0, "");

    for (int k = 0; k < 10; k++) {
        int c = e->x + p * k + (p - 9) / 2, g = g_cfg.eq_gain[k], on = k == s_band;

        g = g < -12 ? -12 : g > 12 ? 12 : g;           /* (a hand-edited GILB.CFG) */
        if (st) {
            for (int s = 1; s <= 6; s++) {             /* segment s: 2s - 1 dB and up */
                ui_fill(c - 7, zl - 1 - 6 * s, 15, 4, g >= 2 * s - 1 ? lit : off);
                ui_fill(c - 7, zl + 2 + 6 * (s - 1), 15, 4, -g >= 2 * s - 1 ? lit : off);
            }
        } else {
            int h = (g < 0 ? -g : g) * (e->h / 2) / 12;

            ui_fill(c - 1, e->y, 3, e->h, on ? t->text_dim : off);
            ui_fill(c - 5, g > 0 ? zl - h : zl, 11, h ? h : 1, on ? t->text : lit);
        }
        if (e->a >= 170)
            continue;
        if (st && on)
            ui_fill(c - 13, e->a - 11, 27, 14, t->sel_bg);
        ui_text(f, st ? c - 13 : c - 10, e->a, st ? 27 : 21, 1, k_hz[k],
                on ? (st ? t->sel_text : t->text) : st ? lit : t->text_dim, e->a - 11, e->a + 2);
    }
}

/* From the layout's [eq] (docs/LAYOUTS.md), parsed before this overlay took the window
 * (startup/ap_ovl.c ap_eq). */
static void draw(void)
{
    lay_ctx_t x = { eq_words, 0, 0, 0, NO_ICON, 0, T->text, 0 };
    int n;
    const lay_el_t *els = lay_screen_els(LS_EQ, &n);

    ui_fill(0, BODY_Y, DISPLAY_W, DISPLAY_H - BODY_Y, T->background);
    lay_screen_draw(LS_EQ, &x, 0);
    for (int i = 0; i < n; i++)
        if (els[i].kind == LK_BAR && els[i].field == LF_EQ)
            bands(&els[i]);
}

/* the page's act (a = A_*), and its draw (a = -1): one entry (startup/ap_ovl.c ap_eq) */
void eq_page(int a, int i)
{
    int8_t *g = &g_cfg.eq_gain[s_band];

    if (a < 0) {
        draw();
        return;
    }
    if (a == A_DONE) {
        s_band = (uint8_t)((s_band + 1u) % 10u);
    } else if (a == A_VOL_UP || a == A_VOL_DN || a == A_DEFAULT) {
        int v = *g < -12 ? -12 : *g > 12 ? 12 : *g;

        *g = (int8_t)(a == A_DEFAULT ? 0 : a == A_VOL_UP ? (v < 12 ? v + 1 : 12) : (v > -12 ? v - 1 : -12));
        g_cfg.eq = 7;                              /* (Custom: what is edited is what is heard) */
        audio_eq();
        config_save_later();
    } else {
        if (a == A_CANCEL)
            nav_pop();
        return;
    }
    ui_invalidate(0, BODY_Y, DISPLAY_W - 1, DISPLAY_H - 1);
}

