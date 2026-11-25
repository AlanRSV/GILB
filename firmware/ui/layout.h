/*
 * layout.h -- screens drawn from layout files (docs/LAYOUTS.md): the parsed elements and the engine's
 * entry points. Phase 1: Now Playing's cover view; phase 2: the list rows and the header; phase 3:
 * the other screens, one at a time ([question], [slider], [info]: parsed when shown, into g_lscr).
 *
 * The parser and Now Playing's drawing (ui/layout.c, with the built-in layouts) are the `lay` overlay;
 * the parsed layout below is in main memory: Now Playing's second-by-second redraw finds its rectangles
 * without the overlay, and the lists, which every mode shows, are drawn from it by main memory's own
 * code (ui/nav.c). The overlay parses once a layout or the Look changes (ap_lay_load).
 */
#ifndef RECHORD_LAYOUT_H
#define RECHORD_LAYOUT_H

#include <stdint.h>

#define LAY_MAX 32                       /* Now Playing's elements */
#define LROW_MAX 12                      /* a list row's */
#define LHDR_MAX 10                      /* the header's parts and title */
#define LSCR_MAX 12                      /* the screen shown's ([question], [slider], [info]) */
#define LAY_STR 160                      /* bytes for a layout's own words (text="...") */
#define LAY_FILES 8                      /* layout files listed (GILB/LAYOUTS/<name>.txt) */
#define LAY_NAME 24
#define LAY_DIR "/Internal/GILB/LAYOUTS"
#define LROW_MIN 18                      /* a list row's height: 18..42 px (ui/frame.h NROWS_MAX rows) */
#define LROW_MAXH 42

enum { LK_NONE, LK_TEXT, LK_COVER, LK_BOX, LK_FRAME, LK_BAR, LK_BADGE,
       LK_MARK, LK_LABEL, LK_VALUE, LK_CHEV, LK_SELECT, LK_SCROLL,      /* [list] */
       LK_PART, LK_TITLE,                                              /* [header] */
       LK_ICON };                                                      /* the screen's icon ([question]) */

/* the other screens (phase 3), one parsed at a time into g_lscr */
enum { LS_NONE, LS_QUESTION, LS_SLIDER, LS_INFO };

/* the header's parts (LK_PART field), packed from the right in their lines' order */
enum { HP_BATTERY, HP_PERCENT, HP_CLOCK, HP_VOLUME, HP_OUTPUT, HP_LOCK, HP_PLAY };

/* text fields (LK_TEXT, LK_BADGE) and bar kinds (LK_BAR) */
enum { LF_WORDS, LF_TITLE, LF_ARTIST, LF_ALBUM, LF_YEAR, LF_QUEUE, LF_ELAPSED, LF_REMAIN, LF_TOTAL, LF_KBPS,
       LF_FACTS, LF_NEXT, LF_CODEC, LF_FORMAT, LF_PROGRESS, LF_VOLUME,
       LF_LINE1, LF_LINE2, LF_LABEL, LF_VALUE, LF_MIN, LF_MAX, LF_NOTE, LF_RESTORE, LF_LEVEL };   /* (phase 3's) */

/* fonts (lay_el_t.font) */
enum { LFN_S8, LFN_B13, LFN_M13, LFN_T17, LFN_D28, LFN_PX };

/* flags */
#define LG_ALIGN   0x03u                 /* 0 left, 1 centre, 2 right */
#define LG_WRAP    0x04u                 /* text: two lines */
#define LG_FLOW    0x08u                 /* text: moved down by a wrapped text's grow */
#define LG_TINT    0x10u                 /* cover: the theme's two colours */
#define LG_PAUSE   0x20u                 /* cover: the paused mark */
#define LG_SOLID   0x40u                 /* cover: no art: a block of its colour; mark: no section icons */
#define LG_TICKS   LG_WRAP               /* bar: a tick a step under it */
#define LG_STEPS   LG_SOLID              /* bar: a box a step */
#define LG_ALT     0x80u                 /* bar, scroll: outline style; badge: outline; chev: a ">" in the
                                          * element's font; text: px falls back to t17 */

/* colours (color, color2): a theme_t field's place (its byte offset / 2), LCOL_PANEL, or LCOL_DEF (the
 * element's own default) */
#define LCOL_TONE  0xFDu                 /* the screen's own colour: a question's (text when ordinary), an
                                          * info line's value (warn when off) */
#define LCOL_PANEL 0xFEu                 /* Retro's Now Playing panel: 14 % from the background to the text */
#define LCOL_DEF   0xFFu

typedef struct {
    uint8_t kind, field, font, flags;
    int16_t x, y, w, h;                  /* text, label, value: y is the baseline; in [list] all from the row's
                                          * top-left */
    uint8_t color, color2;
    uint8_t a, b;                        /* text: up, grow (a wrap), words: their offset in g_lay_str;
                                          * mark: the number's baseline, -; label: the bold font + 1, the gap
                                          * after the mark; value: the gap before it, its widest; chev: the
                                          * room it takes from the row's right; part: the gap before it,
                                          * its font when px lacks a letter (LFN_* + 1); title: the same */
} lay_el_t;

/* The parsed layout (main memory): Now Playing's elements, and a list row's. */
extern lay_el_t g_lay[LAY_MAX];
extern uint8_t g_lay_n;
extern char g_lay_str[LAY_STR];
extern lay_el_t g_lrow[LROW_MAX];
extern uint8_t g_lrow_n, g_lrow_h;      /* (g_lrow_n 0: ui/nav.c's own default row) */
extern lay_el_t g_lhdr[LHDR_MAX];
extern uint8_t g_lhdr_n;                 /* (0: ui/frame.c's own default header) */
extern lay_el_t g_lscr[LSCR_MAX];
extern uint8_t g_lscr_n, g_lscr_id, g_lscr_row;   /* (g_lscr_row: [info]'s line pitch) */
extern int32_t g_lscr_key;               /* the g_lay_key it was parsed for */
extern int32_t g_lay_key;                /* which one is parsed: (look << 16 | name's hash), -1: none */

/* A colour of the theme in use: lay_el_t.color's place, or dflt (LCOL_DEF). */
uint16_t lay_color(unsigned c, uint16_t dflt);
/* An element's font (LFN_*); for s: the pixel font gives way to alt (LFN_* + 1, 0: b13) for what it lacks. */
struct font;
const struct font *lay_font(unsigned f, unsigned alt, const char *s);

/* The lay overlay (startup/ap_ovl.c gates; 0 / < 0 when it cannot be had). */
int ap_lay_np(int band);                 /* Now Playing's cover view from g_lay (loaded first if need
                                          * be); band: the volume or seek band has the band's rows */
int ap_lay_scan(char (*names)[LAY_NAME], int max);   /* the Layout picker's list: "Built-in", then
                                          * GILB/LAYOUTS/'s names; how many | the current one's place << 8 */
int ap_lay_load(void);                   /* the Look's layout parsed into g_lay / g_lrow, if g_lay_key says
                                          * it is not (ui/nav.c: before a list is drawn) */
int ap_lay_screen(int id);               /* a screen's section (LS_*) parsed into g_lscr */

/* A screen drawn from its section (main memory: ui/nav.c): its words from words(field), a bar's level
 * v of lo..hi, its icon (NO_ICON: none) in icon_c, LCOL_TONE as tone; each element moved down dy. */
typedef struct {
    const char *(*words)(int field, char *b, int cap);
    int v, lo, hi, icon;
    uint16_t icon_c, tone;
} lay_ctx_t;
const lay_el_t *lay_screen_els(int id, int *n);
void lay_screen_draw(int id, const lay_ctx_t *c, int dy);

/* the overlay's own */
int lay_np(int band);
int lay_scan(char (*names)[LAY_NAME], int max);
int lay_load(void);
int lay_screen(int id);

#endif /* RECHORD_LAYOUT_H */
