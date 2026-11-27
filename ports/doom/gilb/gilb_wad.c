/*
 * gilb_wad.c -- the "external flash" of extMemory.h: the WAD file, a read cache, the save files.
 *
 * The MG24 read its SPI flash in a few microseconds; a read from the player's storage costs far
 * more per request, so the WAD is read in pages (GILB_WAD_PAGE bytes) kept in RAM, the least
 * recently used one replaced. The renderer's column reads and the level set-up's 4-byte reads then
 * mostly come from RAM.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include <string.h>
#include "gilb_board.h"
#include "extMemory.h"
#include "z_zone.h"

#define WAD_PATH  "/Internal/GILB/APPS/DOOM.WAD"
#define SAVE_PATH "/Internal/GILB/APPS/DOOM.SAV"
#define CFG_PATH  "/Internal/GILB/APPS/DOOM.CFG"
#define SAVE_AT   (GILB_EXT_SIZE - 2u * 4096u)   /* g_game.c's SAVEGAME_ADDRESS */
#define CFG_AT    (GILB_EXT_SIZE - 4u * 4096u)   /* g_game.c's SETTINGS_ADDRESS */
#define MAXPAGES  64

uint32_t currentSpiAddress;

static void *s_wad;
static uint32_t s_wad_len;
static int s_npages;
static uint8_t *s_slot[MAXPAGES];     /* where each slot is: AHI's rest, then a block of the zone */
static uint32_t s_page_no[MAXPAGES];   /* which page of the file each slot holds; ~0: none */
static uint32_t s_page_used[MAXPAGES]; /* when it was last used */
static uint32_t s_clock;
static int s_last;                   /* the slot that served the last read */
uint32_t gilb_wad_misses;            /* storage requests (the frame-rate line shows them) */
uint32_t gilb_wad_ms;                /* ...and the time they took */
#define READ_AHEAD 4
static uint8_t *s_bounce;            /* READ_AHEAD pages, read in one request */

/* Slots of GILB_WAD_PAGE bytes in [p, p + n). */
static void add_slots(uint8_t *p, uint32_t n)
{
    for (; n >= GILB_WAD_PAGE && s_npages < MAXPAGES; n -= GILB_WAD_PAGE, p += GILB_WAD_PAGE)
        s_slot[s_npages++] = p;
}

/* After Z_Init: AHI's rest, and GILB_WAD_ZONE bytes of the zone. */
void extMemInit(void)
{
    s_npages = 0;
    add_slots(gilb_wadcache_base, gilb_wadcache_size);
    s_bounce = Z_Malloc(READ_AHEAD * GILB_WAD_PAGE, PU_STATIC, NULL);
    add_slots(Z_Malloc(GILB_WAD_ZONE - READ_AHEAD * GILB_WAD_PAGE, PU_STATIC, NULL),
              GILB_WAD_ZONE - READ_AHEAD * GILB_WAD_PAGE);
    for (int i = 0; i < MAXPAGES; i++)
        s_page_no[i] = ~0u;
    if ((s_wad = gilb->open(WAD_PATH, GF_READ)) == NULL)
        gilb_error("No GILB/APPS/DOOM.WAD (the converted WAD)");
    s_wad_len = (uint32_t)gilb->size(s_wad);
    gilb_logf("doom: DOOM.WAD %lu B, read cache %u x %u B", (unsigned long)s_wad_len,
              (unsigned)s_npages, (unsigned)GILB_WAD_PAGE);
}

void gilb_wad_close(void)
{
    if (s_wad)
        gilb->close(s_wad);
    s_wad = NULL;
}

static int find(uint32_t pg)
{
    for (int i = 0; i < s_npages; i++)
        if (s_page_no[i] == pg)
            return i;
    return -1;
}

static int lru(void)
{
    int old = 0;

    for (int i = 1; i < s_npages; i++)
        if (s_page_used[i] < s_page_used[old])
            old = i;
    return old;
}

/* The slot holding page pg, read in if it is not there yet. A storage request costs about 0.7 ms
 * however small (device 2026-11-26), and the game reads patches column after column, so a miss
 * brings in up to READ_AHEAD pages that follow, in the same request. */
static uint8_t *page(uint32_t pg)
{
    uint32_t k, at, n, t0;
    int i;

    if (s_page_no[s_last] == pg) {
        s_page_used[s_last] = ++s_clock;
        return s_slot[s_last];
    }
    if ((i = find(pg)) >= 0) {
        s_page_used[i] = ++s_clock;
        s_last = i;
        return s_slot[i];
    }
    for (k = 1; k < READ_AHEAD && (pg + k) * GILB_WAD_PAGE < s_wad_len && find(pg + k) < 0; k++)
        ;
    at = pg * GILB_WAD_PAGE;
    n = k * GILB_WAD_PAGE;
    if (at + n > s_wad_len)
        n = at < s_wad_len ? s_wad_len - at : 0;
    t0 = gilb->ms();
    if (n && (gilb->seek(s_wad, (long)at) != (long)at || gilb->read(s_wad, s_bounce, n) != (long)n))
        gilb_error("Can't read DOOM.WAD at %lu", (unsigned long)at);
    gilb_wad_ms += gilb->ms() - t0;
    gilb_wad_misses++;
    for (uint32_t j = k; j-- > 0; ) {    /* (pg last: it is the most recently used) */
        int s = lru();

        memcpy(s_slot[s], s_bounce + j * GILB_WAD_PAGE, GILB_WAD_PAGE);
        s_page_no[s] = pg + j;
        s_page_used[s] = ++s_clock;
        s_last = s;
    }
    return s_slot[s_last];
}

/* A save file's bytes, or 0xFF (erased) where there are none. */
static void save_read(const char *path, uint32_t off, uint8_t *d, uint32_t n)
{
    void *f = gilb->open(path, GF_READ);
    long got = 0;

    if (f) {
        if (gilb->seek(f, (long)off) == (long)off)
            got = gilb->read(f, d, n);
        gilb->close(f);
    }
    if (got < 0)
        got = 0;
    if ((uint32_t)got < n)
        memset(d + got, 0xFF, n - (uint32_t)got);
}

void gilb_ext_read(uint32_t off, void *dest, uint32_t n)
{
    uint32_t misses0 = gilb_wad_misses;
    uint8_t *d = dest;

    if (off >= CFG_AT) {             /* the save game and the settings */
        if (off >= SAVE_AT)
            save_read(SAVE_PATH, off - SAVE_AT, d, n);
        else
            save_read(CFG_PATH, off - CFG_AT, d, n);
        return;
    }
    while (n && off < 4) {           /* the WAD's length, as the MG24's flash held it */
        *d++ = (uint8_t)(s_wad_len >> (8 * off));
        off++;
        n--;
    }
    off -= 4;
    if (n >= 4u * GILB_WAD_PAGE && s_wad) {   /* a whole lump: straight from the file, past the pages */
        if (gilb->seek(s_wad, (long)off) != (long)off || gilb->read(s_wad, d, n) != (long)n)
            gilb_error("Can't read DOOM.WAD at %lu", (unsigned long)off);
        return;
    }
    while (n) {
        uint32_t in = off % GILB_WAD_PAGE, k = GILB_WAD_PAGE - in;

        if (k > n)
            k = n;
        memcpy(d, page(off / GILB_WAD_PAGE) + in, k);
        d += k;
        off += k;
        n -= k;
    }
    /* A storage read takes a while: keep the sound fed. Here, once the read is done (a mix reads the
     * WAD too: inside page() it took the bounce buffer and slots from under this read). */
    if (gilb_wad_misses != misses0)
        gilb_sound_tick();
}

/* g_game.c writes each record whole, at its start. */
void extMemWrite(uint32_t address, void *buffer, uint32_t size)
{
    uint32_t off = address & SPI_ADDRESS_MASK;
    const char *path = off >= SAVE_AT ? SAVE_PATH : off >= CFG_AT ? CFG_PATH : NULL;
    void *f;

    if (!path || (off != SAVE_AT && off != CFG_AT)) {
        gilb_logf("doom: write to 0x%08lx refused", (unsigned long)address);
        return;
    }
    if ((f = gilb->open(path, GF_WRITE)) == NULL) {
        gilb_logf("doom: can't write %s", path);
        return;
    }
    if (gilb->write(f, buffer, size) != (long)size)
        gilb_logf("doom: %s: short write", path);
    gilb->close(f);
}
