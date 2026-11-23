/*
 * test_emmc_layout.c -- emmc_layout() (drivers/emmc_layout.h, the driver's own code) on a fake
 * 7456 MiB eMMC: the IDB present or not, slots, the "SAVE" block, the user volume at 0x88000.
 * Build once per board: -DBOARD=1 (8 GB: the firmware area from the slots when no IDB is found)
 * and -DBOARD=2 (512 MB: unchanged). Optional: EMMC_DUMP=<file> runs the 8 GB cases on a real
 * rockusb read of the firmware area (rl 0 0x40000: physical 0x2000.., docs/BOARD_DIFF_512M.md).
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "emmc.h"

static uint32_t s_sectors = 15269888u;           /* 7456 MiB, both measured units */
static uint32_t s_buf[256];
static uint32_t s_write_floor = 0xFFFFFFFFu, s_fw_sectors;
uint32_t g_emmc_diag[3];                        /* (the driver's: what the screen shows) */

/* ---- the fake disk: a few sectors set by the test, a real dump, zeros elsewhere ---- */
#define MAXS 64
static struct { uint32_t lba; uint8_t d[512]; } s_set[MAXS];
static int s_n, s_reads;
static uint8_t *s_dump;                          /* physical 0x2000 + [0, s_dump_n) */
static uint32_t s_dump_n;

static uint8_t *sector(uint32_t lba)
{
    for (int i = 0; i < s_n; i++)
        if (s_set[i].lba == lba)
            return s_set[i].d;
    if (s_n == MAXS)
        exit(2);
    s_set[s_n].lba = lba;
    memset(s_set[s_n].d, 0, 512);
    return s_set[s_n++].d;
}

int emmc_read(uint32_t lba, uint32_t count, void *buf)
{
    s_reads++;
    for (uint32_t k = 0; k < count; k++, lba++) {
        uint8_t *o = (uint8_t *)buf + k * 512u;
        int found = 0;

        if (lba >= s_sectors)
            return -1;
        for (int i = 0; i < s_n; i++)
            if (s_set[i].lba == lba) {
                memcpy(o, s_set[i].d, 512);
                found = 1;
            }
        if (found)
            continue;
        if (s_dump && lba >= 0x2000u && (lba - 0x2000u) < s_dump_n / 512u)
            memcpy(o, s_dump + (size_t)(lba - 0x2000u) * 512u, 512);
        else
            memset(o, 0, 512);
    }
    return 0;
}

static char s_log[256];
void log_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_log, sizeof s_log, fmt, ap);
    va_end(ap);
}

#include "emmc_layout.h"

/* ---- the pieces ---- */
static void reset(void)
{
    s_n = s_reads = 0;
    s_write_floor = 0xFFFFFFFFu;
    s_fw_sectors = 0;
    s_log[0] = 0;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void idb(uint32_t mib)                    /* a recognised IDB copy at 0x40 */
{
    uint8_t *a = sector(0x40), *b = sector(0x41);

    put32(a, 0xFCDC8C3Bu);
    b[0x2] = (uint8_t)mib;                        /* +0x202 */
    b[0x3] = (uint8_t)(mib >> 8);
    memcpy(b + 0xA, "NANO", 4);                   /* +0x20A */
}

static void slot(uint32_t lba, uint32_t img_bytes)   /* an image header (and its copy) */
{
    uint8_t *h = sector(lba), *c;

    put32(h + 0x1F4, img_bytes);
    memcpy(h + 0x1F8, "RKnanoFW", 8);
    c = sector(lba + img_bytes / 512u);
    memcpy(c, h, 512);
}

static void fat32(uint32_t lba)                  /* a FAT32 boot sector */
{
    uint8_t *b = sector(lba);

    b[0] = 0xEB;
    b[0x0B] = 0x00;
    b[0x0C] = 0x02;
    b[0x1FE] = 0x55;
    b[0x1FF] = 0xAA;
}

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int layout(uint32_t *user)
{
    *user = 0;
    return emmc_layout(user);
}

int main(void)
{
    uint32_t user;
    int r;

    /* an IDB (0x44 MiB = 0x22000 sectors), "SAVE" at 0x24000, the volume at 0x88000: both boards */
    reset();
    idb(0x44);
    memcpy(sector(0x24000), "SAVE", 4);
    fat32(0x88000);
    r = layout(&user);
    CHECK(r == 0 && user == 0x88000u && s_fw_sectors == 0x22000u && s_write_floor == 0x88000u);
    CHECK(!s_log[0]);                             /* (no fallback taken) */

    /* the factory 8 GB unit: no IDB, the slots at 0x2000 and 0x13000 (5 MB images), SAVE, volume */
    reset();
    slot(0x2000, 0x450000);
    slot(0x2000 + 0x11000, 0x450000);
    memcpy(sector(0x24000), "SAVE", 4);
    fat32(0x88000);
    r = layout(&user);
#if BOARD == BOARD_8G
    CHECK(r == 0 && user == 0x88000u && s_fw_sectors == 0x22000u);
    CHECK(g_emmc_diag[2] == 0x616E4B52u);         /* "RKna": slot 1's header, as the screen would show */
#else
    CHECK(r == -1 && s_fw_sectors == 0 && s_write_floor == 0xFFFFFFFFu);   /* 512 MB: as before */
#endif

#if BOARD == BOARD_8G
    /* stock's leftover 32 MB image header at 0x10000 (that unit's): not slot 2, which matches slot 1 */
    reset();
    slot(0x2000, 0x450000);
    slot(0x2000 + 0x10000, 0x1FA0000);
    slot(0x2000 + 0x11000, 0x450000);
    fat32(0x88000);
    CHECK(layout(&user) == 0 && s_fw_sectors == 0x22000u);

    /* slot 1 exactly 1 MiB long: its header copy at 0x2800 is not taken for slot 2 */
    reset();
    slot(0x2000, 0x100000);
    slot(0x2000 + 0x11000, 0x100000);
    fat32(0x88000);
    CHECK(layout(&user) == 0 && s_fw_sectors == 0x22000u);

    /* no slot 2 (an update cut short): no area, nothing writable */
    reset();
    slot(0x2000, 0x450000);
    fat32(0x88000);
    CHECK(layout(&user) == -1 && s_fw_sectors == 0 && s_write_floor == 0xFFFFFFFFu);

    /* no slot 1 either: the screen's words are what was read (zeros here) */
    reset();
    g_emmc_diag[1] = g_emmc_diag[2] = 0x12345678u;
    fat32(0x88000);
    CHECK(layout(&user) == -1 && g_emmc_diag[1] == 0 && g_emmc_diag[2] == 0);

    /* the area found, no volume after it */
    reset();
    slot(0x2000, 0x450000);
    slot(0x2000 + 0x11000, 0x450000);
    CHECK(layout(&user) == -2 && s_fw_sectors == 0);

    /* the real unit's dump, if given */
    {
        const char *path = getenv("EMMC_DUMP");
        FILE *f = path ? fopen(path, "rb") : NULL;

        if (f) {
            s_dump = malloc(0x40000u * 512u);
            s_dump_n = (uint32_t)fread(s_dump, 1, 0x40000u * 512u, f);
            fclose(f);
            reset();
            fat32(0x88000);                       /* (the volume is past the dump) */
            r = layout(&user);
            printf("dump: %s -> %d, user %x, fw %x (%d reads)\n", path, r, (unsigned)user,
                   (unsigned)s_fw_sectors, s_reads);
            CHECK(r == 0 && user == 0x88000u && s_fw_sectors == 0x22000u);
            free(s_dump);
            s_dump = NULL;
        }
    }
#endif
    printf("test_emmc_layout (BOARD=%d): %d checks, %d failures\n", BOARD, checks, fails);
    return fails != 0;
}
