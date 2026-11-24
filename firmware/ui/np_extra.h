/*
 * np_extra.h -- Now Playing's other views (BACK tap cycles Cover -> Lyrics -> Up Next): the npx
 * overlay (ui/np_extra.c, services/lyrics.c) draws their body from what Now Playing shows (g_np);
 * ui/screens.c keeps the band (the progress bar) and calls in through the gates in
 * startup/ap_ovl.c. The resident side is kept small (docs/MEMORY_MAP.md, the code budget).
 */
#ifndef RECHORD_NP_EXTRA_H
#define RECHORD_NP_EXTRA_H

#include <stdint.h>

#include "services/fs.h"

enum { NPV_COVER = 0, NPV_LYRICS, NPV_NEXT, NPV_SPECTRUM, NPV_COUNT };   /* (Spectrum: the viz overlay) */

/* What Now Playing shows (ui/screens.c's N). */
typedef struct {
    uint32_t seq, pos, dur, until, clus;          /* pos, dur: seconds */
    char title[96], artist[64], album[64], next[64], init[4], codec[8], fmt[12], facts[40];
    uint8_t hue, hires, paused, band, win, err;   /* band: 0 progress, 1 volume, 2 seek */
    uint8_t cover;                                /* the album cover is loaded */
    int16_t head;
} np_now_t;
extern np_now_t g_np;
extern uint16_t g_np_cover[56 * 56];   /* the album cover (cover56.rgb) when g_np.cover (ui/screens.c) */
/* Look: Retro (ui/retro.c, the viz overlay): Now Playing's body (part 0) or its progress band (1);
 * 0 when it cannot be had now (USB-C earphones hold the window): Modern draws it. */
int ap_retro_np(int part);

typedef struct {
    void (*draw)(int v);               /* the body (BODY_Y .. the band) of view v */
    void (*tick)(void);                /* the lyrics redrawn when another line is due */
    void (*splash)(void);              /* the boot splash (startup/ap_ui.c) */
    void (*rg)(const fs_ref_t *ref, int kind, int32_t g[2]);   /* ReplayGain (audio/rg_read.c) */
    long (*queue)(int op);             /* the saved queue (services/queue.c npx_queue) */
} npx_api_t;
long npx_queue(int op);
/* The grid's covers (ui/lib_ui.c fills them as it gives Library > Albums' rows; ui/nav.c PK_GRID draws
 * them): slot i & 3 holds row g_grid_index[slot]'s cover56.rgb, size 0 when it has none. */
extern fs_ref_t g_grid_cover[4];
extern int16_t g_grid_index[4];
/* startup/ap_ovl.c: QUEUE.BIN. op 0 save, 1 load (the time to resume at; < 0: none), 2 check after a
 * USB session that wrote; < 0 also when the overlay cannot be had (USB-C earphones streaming). */
long ap_npx_queue(int op);
void npx_clock(void);
void ap_npx_clock(void);               /* startup/ap_ovl.c: at boot and after a USB disk session */
void rg_read(const fs_ref_t *ref, int kind, int32_t g[2]);

/* The boot splash's picture: SPLASH_PATH on Internal, DISPLAY_W x DISPLAY_H RGB565 little-endian,
 * rows top down, exactly SPLASH_BYTES, no header (cover56.rgb's format, full screen; Echo Library
 * makes it). Without it: RECHORD and the build's date. */
#define SPLASH_PATH  "/Internal/GILB/SPLASH.RGB"
#define SPLASH_BYTES (320u * 170u * 2u)

/* startup/ap_ovl.c, resident.
 * ap_npx_next: the next view (the body redrawn); Cover when the views can't be had now (USB-C
 *   earphones hold the overlay window, or the overlay was refused): "Lyrics: unavailable".
 * ap_npx_draw: 1 when the view (not Cover) was drawn; 0: Now Playing draws the cover.
 * ap_npx_tick: the Lyrics view's tick when the overlay is in the window (never loads it). */
void ap_npx_next(void);
int ap_npx_draw(void);
void ap_npx_tick(void);
void ap_npx_splash(void);              /* (loads the overlay: at boot, before anything else needs it) */
int ap_gilb_move(int vol);             /* RECHORD/ -> GILB/ on volume vol (services/gilb_move.c, the viz overlay) */

#endif
