/*
 * gilb_main.c -- Doom's start on GILB: the first stage, the memory, the way out.
 *
 * GILB loads DOOM.APP (the APP block of doom.ld: 112 KiB at 0x03030000) and calls app_main. The
 * first stage, which needs nothing but the APP block, then:
 *   1. asks for core B's RAM (api->more_mem) and core A's fonts window and PCM ring
 *      (api->more_mem_a, API version 3)
 *   2. reads DOOM.BIN, the other blocks (AHI, BLO, BHI), checks it was built with this DOOM.APP
 *      and its CRC, and zeroes each block's .bss
 *   3. hands out what is left of the blocks: the sound's ring (gilb_sound.c), the game's "internal
 *      flash" (level data, textures: gilb_cache_base) and the WAD's read cache (gilb_wad.c)
 *   4. starts the game (D_DoomMain, which never returns). The way back to GILB is a longjmp: from
 *      gilb_error, or when the user holds POWER for 2 s (api->poll).
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include "gilb_board.h"
#include "printf.h"
#include "d_main.h"
#include "z_zone.h"
#include "global_data.h"
#include "doom_ble.h"
#include "extMemory.h"

#define APP_STAGE1 __attribute__((section(".stage1")))
#define APP_BSS    __attribute__((section(".app_bss")))

/* doom.ld */
extern char __ahi_start[], __ahi_free[], __ahi_end[];
extern char __blo_start[], __blo_free[], __blo_end[];
extern char __bhi_start[], __bhi_free[], __bhi_end[];
extern const gilb_app_hdr_t gilb_hdr;

/* Kept in the APP block: the first stage runs before the others are loaded. */
const gilb_api_t *gilb APP_BSS;
static jmp_buf s_out APP_BSS;
static int s_rc APP_BSS;

uint8_t *gilb_cache_base;
uint32_t gilb_cache_size;
uint8_t *gilb_wadcache_base;
uint32_t gilb_wadcache_size;

/* DOOM.BIN (tools/pack.py): this header, then each block's bytes. */
#define BIN_MAGIC 0x4E49424Du        /* "MBIN" */
typedef struct {
    uint32_t magic, app_crc, crc, nblk;  /* crc: CRC-32 of everything after this header */
    struct { uint32_t addr, filesz, memsz; } blk[4];
} bin_hdr_t;

/* The first stage's own small helpers (nothing outside the APP block yet). */
APP_STAGE1 static void st_text(int row, const char *s)
{
    static uint16_t line[320] APP_BSS;   /* one row at a time: the APP block is precious */

    for (int y = 0; y < 8; y++) {
        gilb->sync();                        /* (the last row's transfer is over) */
        for (int i = 0; i < 320; i++)
            line[i] = 0;
        for (int c = 0; s[c] && c < 39; c++) {
            const uint8_t *g = gilb->glyph(s[c]);

            for (int x = 0; x < 8; x++)
                if (g[y] & (0x80 >> x))
                    line[4 + c * 8 + x] = row ? 0xFFFF : 0xFFE0;
        }
        gilb->blit(0, 8 + row * 14 + y, 320, 1, line, 320);
    }
    gilb->sync();
}

APP_STAGE1 static void st_say(int row, const char *s)
{
    char l[64] = "doom: ";
    int n = 6;

    while (*s && n < 63)
        l[n++] = *s++;
    l[n] = 0;
    gilb->log(l);
    st_text(row, l + 6);
}

APP_STAGE1 static uint32_t st_crc(uint32_t c, const uint8_t *p, uint32_t n)
{
    while (n--) {
        c ^= (uint32_t)*p++ << 24;
        for (int k = 0; k < 8; k++)
            c = c & 0x80000000u ? (c << 1) ^ 0x04C10DB7u : c << 1;
    }
    return c;
}

APP_STAGE1 static void st_wait(void)
{
    gilb_key_t k;

    for (;;) {
        if (gilb->poll())
            return;
        if (gilb->key(&k) && (k.gesture == GE_TAP || k.gesture == GE_HOLD))
            return;
        gilb->sleep_ms(20);
    }
}

/* Is [a, a + n) inside one of the lent ranges? */
APP_STAGE1 static int st_lent(const gilb_mem_t *m, int nm, uint32_t a, uint32_t n)
{
    for (int i = 0; i < nm; i++)
        if (a >= m[i].base && a + n <= m[i].base + m[i].size)
            return 1;
    return 0;
}

/* Steps 1 and 2; the reason it can't, or NULL. */
APP_STAGE1 static const char *st_load(void)
{
    static bin_hdr_t h APP_BSS;
    gilb_mem_t m[6];
    int nm;
    void *f;
    uint32_t crc = 0;

    if (gilb->version < 3 || gilb->api_size < sizeof(gilb_api_t))
        return "This GILB is too old (API 3 needed)";
    nm = gilb->more_mem(m, 4);
    nm += gilb->more_mem_a(m + nm, 2);
    if ((f = gilb->open("/Internal/GILB/APPS/DOOM.BIN", GF_READ)) == NULL)
        return "No GILB/APPS/DOOM.BIN";
    if (gilb->read(f, &h, sizeof h) != (long)sizeof h || h.magic != BIN_MAGIC || h.nblk > 4) {
        gilb->close(f);
        return "DOOM.BIN is damaged";
    }
    if (h.app_crc != gilb_hdr.crc) {
        gilb->close(f);
        return "DOOM.BIN is from another build";
    }
    for (uint32_t i = 0; i < h.nblk; i++) {
        uint8_t *d = (uint8_t *)h.blk[i].addr;

        if (h.blk[i].filesz > h.blk[i].memsz || !st_lent(m, nm, h.blk[i].addr, h.blk[i].memsz)) {
            gilb->close(f);
            return "DOOM.BIN needs memory GILB has not lent";
        }
        if (gilb->read(f, d, h.blk[i].filesz) != (long)h.blk[i].filesz) {
            gilb->close(f);
            return "Can't read DOOM.BIN";
        }
        crc = st_crc(crc, d, h.blk[i].filesz);
        for (uint32_t k = h.blk[i].filesz; k < h.blk[i].memsz; k++)
            d[k] = 0;
        gilb->poll();
    }
    gilb->close(f);
    if (crc != h.crc)
        return "DOOM.BIN is damaged (CRC)";
    return NULL;
}

void gilb_cache_setup(void);         /* gilb_wad.c */
void gilb_video_setup(void);         /* gilb_video.c */

APP_STAGE1 int app_main(const gilb_api_t *api)
{
    const char *why;

    gilb = api;
    st_say(0, "Doom: loading...");
    if ((why = st_load()) != NULL) {
        st_say(1, why);
        st_text(3, "Any key: back");
        st_wait();
        return 1;
    }
    /* Core A at 224 MHz rather than 96 (API 6), core B being in reset now (more_mem in st_load):
       GILB tests it the first time a boot and refuses it if anything is wrong (its log says why). */
    if (gilb->version >= 6 && gilb->api_size >= sizeof(gilb_api_t))
        gilb->log(gilb->fast(1) == 0 ? "doom: core A at 224 MHz" : "doom: core A stays at 96 MHz");
    /* Step 3: the caches. AHI's rest holds the WAD's pages, all of BHI is the game's "flash". */
    gilb_sound_ring = (int32_t *)__ahi_free;   /* (8-aligned: doom.ld) the sound's ring, in core A's
                                                  RAM, which the audio DMA reads */
    gilb_wadcache_base = (uint8_t *)__ahi_free + GILB_SOUND_RING_BYTES;
    gilb_wadcache_size = (uint32_t)((uint8_t *)__ahi_end - gilb_wadcache_base) / GILB_WAD_PAGE * GILB_WAD_PAGE;
    gilb_cache_base = (uint8_t *)__bhi_start;
    gilb_cache_size = (uint32_t)(__bhi_end - __bhi_start) & ~(uint32_t)8191u;
    memset(gilb_cache_base, 0xFF, gilb_cache_size);    /* "erased flash" */
    gilb_video_setup();
    if (setjmp(s_out) == 0) {
        s_rc = 0;
        Z_Init();
        extMemInit();
        InitGlobals();
        D_DoomMain();
    }
    __enable_irq();
    gilb_wad_close();
    return s_rc;
}

/* ---- the way out ---------------------------------------------------------------------------- */

/* api->poll said POWER has been held 2 s. */
void gilb_quit_power(void)
{
    gilb->log("doom: POWER held: back to GILB");
    gilb_quit(0);
}

void gilb_quit(int rc)
{
    s_rc = rc;
    longjmp(s_out, 1);
}

/* GILB's own work during long jobs (the level set-up), every 50 ms at most. */
void sl_system_process_action(void)
{
    static uint32_t last;
    uint32_t now = gilb->ms();

    if (now - last < 50u)
        return;
    last = now;
    if (gilb->poll())
        gilb_quit_power();
}

void gilb_logf(const char *fmt, ...)
{
    char b[96];
    va_list va;

    va_start(va, fmt);
    vsnprintf_(b, sizeof b, fmt, va);
    va_end(va);
    gilb->log(b);
}

/* The game's own errors: a line in the log, and the game goes on (as on the MG24). */
void I_Error(const char *error, ...)
{
    char b[96] = "doom: note: ";
    va_list va;

    va_start(va, error);
    vsnprintf_(b + 12, sizeof b - 12, error, va);
    va_end(va);
    gilb->log(b);
}

/* What can't go on: the message on the screen and in the log, then back to GILB. */
void gilb_error(const char *error, ...)
{
    char b[96] = "doom: ";
    va_list va;

    va_start(va, error);
    vsnprintf_(b + 6, sizeof b - 6, error, va);
    va_end(va);
    gilb->log(b);
    st_text(0, "Doom stopped:");
    st_text(1, b + 6);
    st_text(3, "Any key: back");
    st_wait();
    gilb_quit(2);
}

void gilb_fatal(const char *why)
{
    gilb_error("%s", why);
}

/* printf's character sink (printf() itself is off: DEBUG_OUT_PRINTF). */
void _putchar(char c) { (void)c; }

/* No Bluetooth game: the player's name is fixed. */
char localPlayerName[MAX_HOST_NAME_LENGTH] = "DOOMGUY";
