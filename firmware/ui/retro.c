/*
 * retro.c -- Now Playing in Settings > Display > Look: Retro (the user, 2026-11-24: an old LCD
 * player's screen). In the viz overlay (startup/ap_ovl.c ap_retro_np); ui/screens.c np_draw asks it
 * for the body (part 0) and the progress band (part 1), and draws the volume and seek bands itself.
 *
 *   "3 of 12"            the queue position, over the cover
 *   the cover            cover56.rgb at 1.5x, in the theme's two colours (an LCD's look), or the
 *                        album's initials on a block of the text colour
 *   title, artist, album and year, one line each in GILB's pixel font (PX; T17 for a line PX lacks)
 *   an outlined progress bar, the times at its ends, the bit rate between them
 *
 * Its places keep to Now Playing's own redraw rectangles (np_poll): the bar's fill in rows
 * BAND_Y + 2 .. + 9, the times in x 8..70 and 232..312 below it. One colour does almost
 * everything (the theme's text), as on a one-colour LCD; the LCD theme (theme/themes_builtin.c)
 * is the Look's colours.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "frame.h"
#include "np_extra.h"
#include "audio/audio.h"
#include "drivers/display.h"
#include "drivers/text.h"
#include "services/queue.h"
#include "theme/theme.h"

#define BAND_Y 138                       /* (ui/screens.c's) */
#define CX 8                             /* the cover: 56 px at 1.5x */
#define CY 42
#define CS 84
#define TXR (CX + CS + 10)               /* the text column */
#define TWR (312 - TXR)
#define N_SEQ_TRACK() (g_np.seq && g_np.seq != 0xFFFFFFFFu)   /* (a track, not one starting) */

unsigned retro_year(void);               /* ui/retro_tags.c */

static uint16_t s_pal[32];               /* the cover's tint: dark (the text) .. light (the background) */

static void tint_band(const display_band_t *b)
{
    for (int y = b->ya; y <= b->yb; y++) {
        const uint16_t *src = g_np_cover + (y - CY) * 2 / 3 * 56;
        uint16_t *dst = b->band + (y - b->band_y) * DISPLAY_W;

        for (int x = b->cx0; x <= b->cx1; x++) {
            uint16_t p = src[(x - CX) * 2 / 3];
            unsigned l = ((p >> 11) * 8u * 77u + ((p >> 5) & 63u) * 4u * 150u + (p & 31u) * 8u * 29u) >> 8;

            dst[x] = s_pal[l >> 3];
        }
    }
}

/* GILB's pixel font (tools/gilb_pixfont.py) holds ASCII, curly quotes and dashes: a line with
 * anything else (É, ô) is drawn in T17, the size it stands in for, rather than with boxes. */
static const font_t *px_or_t17(const char *s)
{
    return text_has_all(&font_px, s) ? &font_px : &font_t17;
}

static void fmt_t(char *o, int cap, uint32_t s)
{
    if (s >= 3600u)
        snprintf(o, (unsigned)cap, "%u:%02u:%02u", (unsigned)(s / 3600u), (unsigned)(s / 60u % 60u), (unsigned)(s % 60u));
    else
        snprintf(o, (unsigned)cap, "%u:%02u", (unsigned)(s / 60u), (unsigned)(s % 60u));
}

static void body(const theme_t *t)
{
    char b[24];
    int n = queue_count();

    if (n > 0 && N_SEQ_TRACK()) {
        snprintf(b, sizeof b, "%d of %d", queue_pos() + 1, n);
        ui_text(&font_px, CX, 37, 160, 0, b, t->text, BODY_Y, CY - 1);
    }
    if (g_np.cover) {
        for (int i = 0; i < 32; i++)
            s_pal[i] = ui_mix(t->text, t->background, i * 8 + 4);
        display_custom(CX, CY, CX + CS - 1, CY + CS - 1, tint_band, 0, 1);
    } else {
        int w = text_width(&font_d28, g_np.init, -1);

        ui_fill(CX, CY, CS, CS, t->text);
        ui_text(&font_d28, CX + (CS - w) / 2, CY + CS / 2 + 10, w, 0, g_np.init, t->background, CY, CY + CS - 1);
    }
    {
        unsigned y = N_SEQ_TRACK() ? retro_year() : 0u;
        const char *ln[4] = { g_np.title, g_np.artist, g_np.album, "" };

        if (y)
            snprintf(b, sizeof b, "%u", y), ln[3] = b;
        for (int i = 0; i < 4; i++)
            ui_text(px_or_t17(ln[i]), TXR, CY + 16 + 22 * i, TWR, 0, ln[i], t->text, BODY_Y, BAND_Y - 1);
    }
}

static void band(const theme_t *t)
{
    char a[12], b[16];
    uint32_t pos = g_np.pos, dur = g_np.dur;
    int hx = dur ? 8 + (int)(304u * (pos < dur ? pos : dur) / dur) : 8;
    unsigned kb = (unsigned)atoi(g_np.facts);   /* ("1411 KBPS · 41.6 MB": np_load) */
    const char *c = g_np.codec;

    ui_frame_rect(8, BAND_Y + 1, 304, 10, t->text);
    if (hx > 10)
        ui_fill(10, BAND_Y + 3, (hx < 310 ? hx : 310) - 10, 6, t->bar_fill);
    fmt_t(a, sizeof a, pos);
    ui_text(&font_px, 8, BAND_Y + 29, 62, 0, a, t->text, BAND_Y + 11, DISPLAY_H - 1);
    if (dur) {
        b[0] = '-';
        fmt_t(b + 1, sizeof b - 1, dur > pos ? dur - pos : 0);
        ui_text(&font_px, 232, BAND_Y + 29, 80, 2, b, t->text, BAND_Y + 11, DISPLAY_H - 1);
    }
    if (kb && strchr(g_np.facts, 'K')) {   /* (no bit rate: the facts are the size alone, "41.6 MB") */
        /* lossless and Vorbis/Opus are variable by nature; an MP3 or AAC's rate is shown as it is */
        int vbr = !strcmp(c, "FLAC") || !strcmp(c, "ALAC") || !strcmp(c, "APE") || !strcmp(c, "WV") ||
                  !strcmp(c, "OGG") || !strcmp(c, "OPUS");

        snprintf(b, sizeof b, vbr ? "%u kbps vbr" : "%u kbps", kb);
        ui_text(&font_s8, 72, BAND_Y + 26, 158, 1, b, t->text, BAND_Y + 11, DISPLAY_H - 1);
    }
}

/* part 0: the body (BODY_Y .. BAND_Y - 1, its background already filled); 1: the progress band. */
int retro_np(int part)
{
    const theme_t *t = theme_get();

    if (part)
        band(t);
    else
        body(t);
    return 1;
}
