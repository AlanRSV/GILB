/*
 * gilb_app.h -- GILB programs: the file format and the functions GILB gives a program.
 *
 * Shared by the firmware (firmware/services/programs.c, the loader) and every program (sdk/).
 * docs/PROGRAMS.md has the whole story; sdk/README.md how to build one.
 *
 * A program is a file NAME.APP in GILB/APPS on the player's internal storage. GILB reads it to
 * GILB_APP_BASE (linked there: sdk/app.ld), checks it, clears its .bss and calls its entry with a
 * pointer to gilb_api_t. While it runs it owns the screen, the keys and GILB_APP_BASE..GILB_APP_END
 * (music stops first; the library's code and the file writer's leave that memory and load again
 * afterwards). When the entry returns, GILB is back where it was: no restart.
 *
 * Compatibility: GILB_API_VERSION grows when functions are added at the end of gilb_api_t; nothing
 * is ever removed or moved. A program sets api_min to the version it needs and checks api->version
 * for anything newer it can do without.
 */
#ifndef GILB_APP_H
#define GILB_APP_H
#include <stdint.h>

#define GILB_APP_MAGIC   0x50504147u    /* "GAPP" */
#define GILB_API_VERSION 5u
#define GILB_APP_BASE    0x03030000u    /* where a program is loaded and runs */
#define GILB_APP_END     0x0304C000u    /* 112 KiB: code, data, .bss, heap */
#define GILB_APP_DIR     "/Internal/GILB/APPS"

/* The first 64 bytes of a .APP file (sdk/crt0.c fills it, sdk/tools/gilb_app.py sets crc). */
typedef struct {
    uint32_t magic;          /* GILB_APP_MAGIC */
    uint32_t hdr_size;       /* sizeof(gilb_app_hdr_t): 64 */
    uint32_t api_min;        /* the GILB_API_VERSION it needs */
    uint32_t base;           /* GILB_APP_BASE (where it was linked) */
    uint32_t load_end;       /* base + the file's length */
    uint32_t mem_end;        /* end of its .bss: load_end..mem_end are zeroed */
    uint32_t entry;          /* int entry(const gilb_api_t *), a Thumb address */
    uint32_t crc;            /* CRC-32 (poly 0x04C10DB7, MSB first, init 0) of the file, this field as 0 */
    char name[32];           /* shown in the list (UTF-8, NUL-terminated); empty: the file's name */
} gilb_app_hdr_t;

/* Keys (the player's buttons). */
enum { GK_NONE = 0, GK_VOLUP = 1, GK_VOLDOWN = 2, GK_PLAY = 3, GK_BACK = 4, GK_POWER = 5, GK_MENU = 6,
       GK_CHORD = 7 /* VOL+ and VOL- together */ };
/* What happened to it (GILB's gesture recogniser, firmware/services/input.h):
 *   GE_DOWN     pressed (for a game: held from here...)
 *   GE_TAP      released quickly (...to here; taps = 1, 2, 3 for double and triple taps)
 *   GE_HOLD     still held after 0.5 s
 *   GE_REPEAT   VOL+ / VOL- still held: steps to move now
 *   GE_RELEASE  released after a HOLD or REPEAT (...or to here)
 *   GE_END      a multi-tap finished (taps = how many) */
enum { GE_NONE = 0, GE_DOWN, GE_TAP, GE_HOLD, GE_REPEAT, GE_RELEASE, GE_END, GE_CHORD };

typedef struct {
    uint8_t key;             /* GK_* */
    uint8_t gesture;         /* GE_* */
    uint8_t taps;
    uint8_t steps;
    uint32_t t_ms;           /* when (api->ms() time) */
} gilb_key_t;

/* A range of memory (more_mem) */
typedef struct {
    uint32_t base, size;
} gilb_mem_t;

/* open() modes */
enum { GF_READ = 0, GF_WRITE = 1 /* create or empty */, GF_APPEND = 2 };

typedef struct gilb_api {
    uint32_t version;        /* GILB_API_VERSION of this firmware */
    uint32_t api_size;       /* sizeof(gilb_api_t) of this firmware */
    int board;               /* 0 = the 8 GB board, 1 = the 512 MB board */
    int screen_w, screen_h;  /* 320 x 170 */

    /* ---- the screen: RGB565 pixels, (0, 0) top left ----
     * blit: w x h pixels from px (rows `pitch` pixels apart) to x, y; clipped to the screen.
     * The transfer may still be reading px when it returns: change those pixels only after the
     * next blit or sync(). */
    void (*blit)(int x, int y, int w, int h, const uint16_t *px, int pitch);
    void (*sync)(void);
    void (*backlight)(int percent);      /* 0..100 */
    /* The 8x8 font: 8 bytes, a byte per row, bit 7 = the leftmost pixel; ' '..'~' (else '?'). */
    const uint8_t *(*glyph)(int ch);

    /* ---- keys and time ----
     * key: the oldest key event into *ev: 1, or 0 when there is none.
     * poll: call it at least every 100 ms (each frame): keeps the player's own work going (the
     * storage's queued writes, the log, Bluetooth, the watchdog). Returns 1 when POWER has been
     * held for 2 s: the user wants out, so return from the entry. */
    int (*key)(gilb_key_t *ev);
    int (*poll)(void);
    uint32_t (*ms)(void);                /* milliseconds since power-on */
    void (*sleep_ms)(uint32_t ms);       /* waits, polling */

    /* ---- files: "/Internal/..." (the internal storage) or "/SD/..." (the microSD card) ----
     * New files' names: 8.3 (NAME.EXT) on the internal storage while a program runs. */
    void *(*open)(const char *path, int mode);   /* GF_*; NULL: no such file, or no room */
    long (*read)(void *f, void *buf, unsigned long n);
    long (*write)(void *f, const void *buf, unsigned long n);
    long (*seek)(void *f, long offset);          /* from the start */
    long (*size)(void *f);
    int (*close)(void *f);
    int (*exists)(const char *path);

    /* ---- the rest ---- */
    void (*log)(const char *line);       /* a line in GILB's log (ECHOLOG.TXT on the internal storage) */
    const char *dir;                     /* GILB_APP_DIR: where the program's file is */

    /* ---- version 2 ---- */
    /* More memory: the audio core's RAM, which is free while a program runs (the audio core is held
     * and loads again at the next play). Fills up to max ranges into out and returns how many: two
     * today, 0x01000000 (128 KiB) and 0x01021000 (252 KiB), around the shared block GILB keeps at
     * 0x01020000 (never write there). Memory only: its contents are undefined and code copied there
     * is not yet known to run (sdk/examples/memtest finds out). */
    int (*more_mem)(gilb_mem_t *out, int max);

    /* ---- version 3 ---- */
    /* More of core A's own RAM: the fonts window, the audio DMA's list and the PCM ring, one range
     * today, 0x03077900..0x030A0000 (161.75 KiB). The music stops and the audio DMA with it (the
     * next play sets both up again); GILB draws only with its built-in 8x8 font (glyph) until the
     * program ends, and then loads its fonts again. Fills up to max ranges and returns how many. */
    int (*more_mem_a)(gilb_mem_t *out, int max);

    /* ---- version 4 ---- */
    /* A USB serial port (CDC-ACM: a COM port on the PC, Windows' own driver), while the program
     * runs and a cable is in. serial_open: 0, or -1 with no cable, -2 when the port is the USB-C
     * earphones'. serial_ready: 1 once the PC has set the port up. serial_read: the bytes the PC
     * sent (up to n; 0: none yet), serial_write: sends n bytes; both -1 when the port is not open.
     * The PC's requests are answered in poll(), so call it often (each frame). GILB closes the port
     * when the program ends. tools/gilb_keys.ps1 sends a PC's keys this way (docs/PROGRAMS.md). */
    int (*serial_open)(void);
    int (*serial_ready)(void);
    long (*serial_read)(void *buf, unsigned long n);
    long (*serial_write)(const void *buf, unsigned long n);

    /* ---- version 5 ---- */
    /* Sound out of the headphone jack. The program owns a ring of blocks x block_frames frames in its
     * own memory (the 112 KiB, or what more_mem_a lent: core A's RAM, which the audio DMA reads), two
     * int32 words a frame, L then R, the sample in the top bits (a 16-bit sample << 16). sound_open
     * stops the music, brings the DAC up at rate (32000, 44100, 48000, 88200, 96000, 176400 or 192000
     * Hz: the DAC, a CS43131, plays nothing below 32 kHz, so upsample: Doom mixes at 11025 Hz and
     * writes each sample four times, interpolated, at 44100), clears the ring and plays it in a loop,
     * block by block, at the player's volume: 0, -1 a bad ring (2..16 blocks of 16..4096 frames,
     * 8-byte aligned) or rate, -2 the DAC failed.
     * sound_pos: the frame the DMA reads now; write the frames after it, ahead of it, as it moves.
     * sound_close stops it (GILB also does when the program ends). sdk/API.md has the details. */
    int (*sound_open)(uint32_t rate, int32_t *ring, uint32_t block_frames, uint32_t blocks);
    uint32_t (*sound_pos)(void);
    void (*sound_close)(void);
} gilb_api_t;

#endif
