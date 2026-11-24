/*
 * spectrum.c -- Now Playing's Spectrum view (BACK tap: Cover -> Lyrics -> Up Next -> Spectrum), in its
 * own overlay (viz, startup/ap_ovl.c): only loaded while it is on screen.
 *
 * Each tick (Now Playing's poll, about 25 a second) takes the 256 frames in the middle of the PCM
 * block the DAC is playing (audio_pcm_window: what is heard now), mono, a Hann window, a fixed-point
 * 256-point FFT, and 24 bars over log-spaced bins; a bar rises at once and falls 4 px a tick, its peak
 * cap holds 12 ticks then falls. Drawn by one custom band op (drivers/display.h display_custom):
 * every pixel of the box from the bars, no op per bar. Nothing is computed while the view is not up.
 */
#include <stdint.h>
#include <string.h>

#include "audio/audio.h"
#include "drivers/display.h"
#include "ui/frame.h"
#include "ui/np_extra.h"
#include "ui/ui.h"
#include "theme/theme.h"
#include "services/config.h"

#define NFFT  256
#define NBAR  24
#define BAR_W 10
#define BAR_P 12                       /* a bar and its gap */
#define X0    ((DISPLAY_W - NBAR * BAR_P) / 2 + 1)
#define Y0    (BODY_Y + 8)             /* the box: the bars stand on Y1 */
#define Y1    133                      /* (the band starts at 138: ui/screens.c) */
#define BAR_H (Y1 - Y0 - 3)            /* the tallest bar, its cap above it */

/* sin(2 pi i / 256), i = 0..64, Q15 */
static const int16_t k_sin[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793, 12539,
    13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005,
    22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898,
    29268, 29621, 29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412,
    32521, 32609, 32678, 32728, 32757, 32767,
};
/* the bins of each bar (bar i: bins k_edge[i] .. k_edge[i + 1] - 1; a bin is rate / 256 wide, 172 Hz
 * at 44.1 kHz): about 170 Hz to 16 kHz, closer than octaves at the bottom (a 256-point FFT has one
 * bin there), wider at the top where music has little */
static const uint8_t k_edge[NBAR + 1] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 13, 15, 18, 21, 24, 28, 32, 37, 43, 49, 56, 64, 73, 83, 94,
};

static int16_t s_re[NFFT], s_im[NFFT];
static uint8_t s_bar[NBAR], s_peak[NBAR], s_hold[NBAR];

static int isin(unsigned k)            /* sin(2 pi k / 256), Q15 */
{
    k &= 255u;
    return k < 64u ? k_sin[k] : k < 128u ? k_sin[128u - k] : k < 192u ? -k_sin[k - 128u] : -k_sin[256u - k];
}

static void fft(void)
{
    for (unsigned i = 1, j = 0; i < NFFT; i++) {   /* bit reversal */
        unsigned b = NFFT >> 1;

        for (; j & b; b >>= 1)
            j ^= b;
        j |= b;
        if (i < j) {
            int16_t t = s_re[i];

            s_re[i] = s_re[j];
            s_re[j] = t;
        }
    }
    memset(s_im, 0, sizeof s_im);
    for (unsigned size = 2; size <= NFFT; size <<= 1) {   /* halved each stage: no overflow */
        unsigned half = size >> 1, step = NFFT / size;

        for (unsigned i = 0; i < NFFT; i += size)
            for (unsigned j = 0; j < half; j++) {
                int wr = isin(j * step + 64u), wi = -isin(j * step);
                unsigned a = i + j, b = a + half;
                int tr = (s_re[b] * wr - s_im[b] * wi) >> 15, ti = (s_re[b] * wi + s_im[b] * wr) >> 15;

                s_re[b] = (int16_t)((s_re[a] - tr) >> 1);
                s_im[b] = (int16_t)((s_im[a] - ti) >> 1);
                s_re[a] = (int16_t)((s_re[a] + tr) >> 1);
                s_im[a] = (int16_t)((s_im[a] + ti) >> 1);
            }
    }
}

/* A bin's magnitude as a height: quarter octaves (1.5 dB) of |re| + |im|, 40 of them (60 dB) tall;
 * bar b lifted b / 3 steps (up to 10.5 dB at the top: music's spectrum falls with frequency) */
static int height(unsigned m, int b)
{
    int msb = 31 - __builtin_clz(m | 1u), lv = 4 * msb + (int)(msb >= 2 ? (m >> (msb - 2)) & 3u : 0u);

    lv += b / 3 - 10;
    return lv <= 0 ? 0 : lv >= 40 ? BAR_H : lv * BAR_H / 40;
}

void viz_tick(void)
{
    int ok = audio_pcm_window(s_re, NFFT) > 0;

    if (ok) {
        for (unsigned i = 0; i < NFFT; i++) {      /* Hann */
            int w = (32767 - isin(i + 64u)) >> 1;

            s_re[i] = (int16_t)(s_re[i] * w >> 15);
        }
        fft();
    }
    for (int b = 0; b < NBAR; b++) {
        int h = 0;

        for (unsigned k = k_edge[b]; ok && k < k_edge[b + 1]; k++) {
            int re = s_re[k], im = s_im[k], v = height((unsigned)((re < 0 ? -re : re) + (im < 0 ? -im : im)), b);

            h = v > h ? v : h;
        }
        s_bar[b] = (uint8_t)(h >= s_bar[b] ? h : s_bar[b] > 4 ? s_bar[b] - 4 : 0);
        if (s_bar[b] >= s_peak[b]) {
            s_peak[b] = s_bar[b];
            s_hold[b] = 12;
        } else if (s_hold[b]) {
            s_hold[b]--;
        } else {
            s_peak[b] = s_peak[b] > 2 ? s_peak[b] - 2 : 0;
        }
    }
    ui_invalidate(X0, Y0, X0 + NBAR * BAR_P - 1, Y1);
}

static void band(const display_band_t *d)
{
    const theme_t *t = T;

    for (int y = d->ya; y <= d->yb; y++) {
        uint16_t *row = d->band + (y - d->band_y) * DISPLAY_W;
        int up = Y1 - y;                           /* rows above the base */

        if (g_cfg.look) {                          /* Retro: an LCD's segments, 3 rows lit and 1 gap; the
                                                      unlit ones faint (text_off), the peak one segment */
            int seg = up >> 2, gap = (up & 3) == 3 || up >= (BAR_H & ~3);

            for (int x = d->cx0; x <= d->cx1; x++) {
                int b = (x - X0) / BAR_P, in = (x - X0) % BAR_P < BAR_W;

                row[x] = !in || b >= NBAR || gap ? t->background
                       : seg < (s_bar[b] + 3) >> 2 || (s_peak[b] && seg == s_peak[b] >> 2) ? t->text
                       : t->text_off;
            }
            continue;
        }
        for (int x = d->cx0; x <= d->cx1; x++) {
            int b = (x - X0) / BAR_P, in = (x - X0) % BAR_P < BAR_W;

            row[x] = !in || b >= NBAR ? t->background
                   : up < s_bar[b] ? t->accent
                   : s_peak[b] && up >= s_peak[b] + 1 && up <= s_peak[b] + 2 ? t->text
                   : t->background;
        }
    }
}

void viz_draw(void)
{
    display_custom(X0, Y0, X0 + NBAR * BAR_P - 1, Y1, band, 0, 1);
}
