/*
 * modules.c -- the module table at MODULES_OFF in our image (tools/make_fw1.py):
 * "RCMOD1\0\0", u32 count, then { char name[8]; u32 off, len, sum, 0 } entries;
 * off counts from the start of the image, sum = u32 sum of its words. Two sectors, 41 entries (one,
 * 20, before 2026-11-23: an image of then reads the same, its count ends the search).
 */
#include <string.h>

#include "modules.h"
#include "drivers/emmc.h"
#include "services/log.h"

#define MODULES_OFF 0x100000u

int module_load(const char *name, void *dst, uint32_t max)
{
    return module_load_slot(0, name, dst, max);
}

int module_load_slot(unsigned slot, const char *name, void *dst, uint32_t max)
{
    uint32_t sec[256], off = 0, len = 0, want = 0, sum = 0;
    uint32_t base = EMMC_FW_BASE + (slot ? emmc_fw_sectors() / 2u : 0u);   /* (fw_update.c's slot 2) */
    const uint8_t *t = (const uint8_t *)sec;

    if (emmc_read(base + MODULES_OFF / 512u, 2, sec) || memcmp(t, "RCMOD1\0\0", 8))
        return -1;
    for (uint32_t i = 0; i < sec[2] && i < 41u; i++) {
        const uint32_t *e = sec + 4 + i * 6u;

        if (!strncmp((const char *)e, name, 8)) {
            off = e[2];
            len = e[3];
            want = e[4];
            break;
        }
    }
    if (!len)
        return -2;
    if (len > max)
        return -3;
    if (emmc_read(base + off / 512u, (len + 511u) / 512u, dst))
        return -1;
    for (uint32_t i = 0; i < len / 4u; i++)
        sum += ((const uint32_t *)dst)[i];
    if (sum != want) {
        log_printf("module %s: sum %08x, want %08x", name, (unsigned)sum, (unsigned)want);
        return -4;
    }
    return (int)len;
}
