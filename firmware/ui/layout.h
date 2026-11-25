/*
 * layout.h -- screens drawn from layout files (docs/LAYOUTS.md): the parsed elements and the engine's
 * entry points. Phase 1: Now Playing's cover view.
 *
 * The engine (ui/layout.c: the parser, the drawing, the built-in layouts) is the `lay` overlay; the
 * parsed layout below is in main memory, so Now Playing's second-by-second redraw finds its rectangles
 * without loading it (ui/screens.c np_poll).
 */
#ifndef RECHORD_LAYOUT_H
#define RECHORD_LAYOUT_H

#include <stdint.h>

#define LAY_MAX 32                       /* elements a screen */
#define LAY_STR 160                      /* bytes for a layout's own words (text="...") */
#define LAY_FILES 8                      /* layout files listed (GILB/LAYOUTS/<name>.txt) */
#define LAY_NAME 24
#define LAY_DIR "/Internal/GILB/LAYOUTS"

enum { LK_NONE, LK_TEXT, LK_COVER, LK_BOX, LK_FRAME, LK_BAR, LK_BADGE };

/* text fields (LK_TEXT, LK_BADGE) and bar kinds (LK_BAR) */
enum { LF_WORDS, LF_TITLE, LF_ARTIST, LF_ALBUM, LF_YEAR, LF_QUEUE, LF_ELAPSED, LF_REMAIN, LF_TOTAL, LF_KBPS,
       LF_FACTS, LF_NEXT, LF_CODEC, LF_FORMAT, LF_PROGRESS, LF_VOLUME };

/* flags */
#define LG_ALIGN   0x03u                 /* 0 left, 1 centre, 2 right */
#define LG_WRAP    0x04u                 /* text: two lines */
#define LG_FLOW    0x08u                 /* text: moved down by a wrapped text's grow */
#define LG_TINT    0x10u                 /* cover: the theme's two colours */
#define LG_PAUSE   0x20u                 /* cover: the paused mark */
#define LG_SOLID   0x40u                 /* cover: no art: a block of its colour (else the album's hue) */
#define LG_ALT     0x80u                 /* bar: outline style; badge: outline; text: px falls back to t17 */

typedef struct {
    uint8_t kind, field, font, flags;
    int16_t x, y, w, h;                  /* text: y is the baseline; h unused */
    uint8_t color, color2;               /* theme roles (layout.c's table); 0xFF: the element's default */
    uint8_t a, b;                        /* text: up, grow (a wrap); words: their offset in g_lay_str */
} lay_el_t;

/* The parsed Now Playing layout (main memory). */
extern lay_el_t g_lay[LAY_MAX];
extern uint8_t g_lay_n;
extern char g_lay_str[LAY_STR];
extern int32_t g_lay_key;                /* which one is parsed: (look << 16 | name's hash), -1: none */

/* The lay overlay (startup/ap_ovl.c gates; 0 / < 0 when it cannot be had). */
int ap_lay_np(int band);                 /* Now Playing's cover view from g_lay (loaded first if need
                                          * be); band: the volume or seek band has the band's rows */
int ap_lay_scan(char (*names)[LAY_NAME], int max);   /* the Layout picker's list: "Built-in", then
                                          * GILB/LAYOUTS/'s names; how many | the current one's place << 8 */

/* the overlay's own */
int lay_np(int band);
int lay_scan(char (*names)[LAY_NAME], int max);

#endif /* RECHORD_LAYOUT_H */
