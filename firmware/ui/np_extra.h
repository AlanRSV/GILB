/*
 * np_extra.h -- Now Playing's other views (BACK tap cycles Cover -> Lyrics -> Up Next): the npx
 * overlay (ui/np_extra.c, services/lyrics.c) draws their body from what Now Playing shows (g_np);
 * ui/screens.c keeps the band (the progress bar) and calls in through the gates in
 * startup/ap_ovl.c. The resident side is kept small (docs/MEMORY_MAP.md, the code budget).
 */
#ifndef RECHORD_NP_EXTRA_H
#define RECHORD_NP_EXTRA_H

#include <stdint.h>

enum { NPV_COVER = 0, NPV_LYRICS, NPV_NEXT, NPV_COUNT };

/* What Now Playing shows (ui/screens.c's N). */
typedef struct {
    uint32_t seq, pos, dur, until, clus;          /* pos, dur: seconds */
    char title[96], artist[64], album[64], next[64], init[4], codec[8], fmt[12], facts[40];
    uint8_t hue, hires, paused, band, win, err;   /* band: 0 progress, 1 volume, 2 seek */
    uint8_t cover;                                /* the album cover is loaded */
    int16_t head;
} np_now_t;
extern np_now_t g_np;

typedef struct {
    void (*draw)(int v);               /* the body (BODY_Y .. the band) of view v */
    void (*tick)(void);                /* the lyrics redrawn when another line is due */
} npx_api_t;

/* startup/ap_ovl.c, resident.
 * ap_npx_next: the next view (the body redrawn); Cover when the views can't be had now (USB-C
 *   earphones hold the overlay window, or the overlay was refused): "Lyrics: unavailable".
 * ap_npx_draw: 1 when the view (not Cover) was drawn; 0: Now Playing draws the cover.
 * ap_npx_tick: the Lyrics view's tick when the overlay is in the window (never loads it). */
void ap_npx_next(void);
int ap_npx_draw(void);
void ap_npx_tick(void);

#endif
