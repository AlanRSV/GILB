/*
 * emmc_layout.h -- where the firmware area and the user volume are on the eMMC (emmc_layout(),
 * drivers/emmc.h). Included by emmc_rknanoc.c only, where it is compiled as part of the driver
 * (the same code, the same size), and by drivers/tests/test_emmc_layout.c, which gives it a disk
 * image instead. The includer provides: s_sectors, s_buf[256] (uint32_t), s_write_floor,
 * s_fw_sectors, emmc_read() and, on the 8 GB board, g_emmc_diag.
 *
 * Physical layout, both units measured (docs/HANDOVER.md verdict #22, docs/BOARD_DIFF_512M.md):
 * the IDB at 0x40 (copies every 0x400), the firmware area from EMMC_FW_BASE (0x2000) for the
 * IDB's system size (0x22000 sectors), slot 1 at its start and slot 2 half way, a "SAVE" block
 * right after it, the user volume further on (0x88000).
 */
#ifndef RECHORD_EMMC_LAYOUT_H
#define RECHORD_EMMC_LAYOUT_H

/* exFAT/FAT boot sector or an MBR with a first partition (reference
 * 0x03052a1c). */
static int is_volume_start(uint32_t lba)
{
    const uint8_t *b = (const uint8_t *)s_buf;

    if (lba >= s_sectors || emmc_read(lba, 1, s_buf) ||
        b[0x1FE] != 0x55 || b[0x1FF] != 0xAA)
        return 0;
    if (memcmp(b + 3, "EXFAT   ", 8) == 0 ||
        ((b[0] & 0xFD) == 0xE9 && (b[0x0B] | b[0x0C] << 8) == 0x200))
        return 1;
    return (b[0x1BE] & 0x7F) == 0 && b[0x1C2] != 0 &&
           (b[0x1C6] | b[0x1C7] | b[0x1C8] | b[0x1C9]) != 0;
}

#if BOARD == BOARD_8G
/* The firmware area from its two slots, when no IDB is recognised: a factory 8 GB unit on stock
 * 4.0.0 (2026-11-22) has none in the form below at 0x40, though its area is the same 0x22000
 * sectors as on the unit the IDB was read from (docs/BOARD_DIFF_512M.md). Slot 1 starts at
 * EMMC_FW_BASE with an image header ("RKnanoFW" at +0x1F8, its size in bytes at +0x1F4, where a
 * copy of the header sits too); slot 2 starts half way with the same header, byte for byte (one
 * update writes both). The half is the first 1 MiB step past slot 1's image whose header is that
 * one: stock leaves other images' headers in the area (that unit: a 32 MB one at 0x10000, its
 * copies at 0xFD00 and 0x1FD00). 0 when slot 1 has no header or no slot 2 matches it. */
static uint32_t fw_from_slots(void)
{
    uint32_t img;

    if (emmc_read(EMMC_FW_BASE, 1, s_buf))
        return 0;
    g_emmc_diag[2] = s_buf[0x1F8 / 4];  /* ("RKna": a header; shown when no layout is found) */
    if (memcmp((const uint8_t *)s_buf + 0x1F8, "RKnanoFW", 8))
        return 0;
    img = s_buf[0x1F4 / 4] >> 9;         /* (its header copy at img: never taken for slot 2) */
    for (uint32_t half = 0x800u; half <= 0x40000u; half += 0x800u) {   /* (the 7456 MiB part's area: 0x22000) */
        if (half > img && !emmc_read(EMMC_FW_BASE + half, 1, s_buf + 128) && !memcmp(s_buf, s_buf + 128, 512))
            return 2u * half;
    }
    return 0;
}
#endif

/* IDB copies at LBA 0x40 + n*0x400: sector 0 magic (RC4'd 0x0FF0AA55),
 * "NANO" chip tag at +0x20A, system size in MiB at +0x202. The volume
 * normally starts right after the firmware area; otherwise the reference
 * scans forward in 1 MiB steps (this unit: 0x24000 holds a "SAVE" block,
 * the volume is at 0x88000). */
int emmc_layout(uint32_t *user_start)
{
    const uint8_t *b = (const uint8_t *)s_buf;
    uint32_t fw = 0;

    for (uint32_t n = 0; n < 5u && !fw; n++) {
        if (emmc_read(0x40u + n * 0x400u, 2, s_buf))
            continue;
#if BOARD == BOARD_8G
        if (!n)
            g_emmc_diag[1] = s_buf[0];     /* (the word at 0x40: shown when no layout is found) */
#endif
        if (s_buf[0] != 0xFCDC8C3Bu || memcmp(b + 0x20A, "NANO", 4) != 0)
            continue;
        fw = (uint32_t)(b[0x202] | b[0x203] << 8) * 0x800u;
        if (fw > s_sectors - 0x2000u)
            fw = 0;
    }
#if BOARD == BOARD_8G
    if (!fw)
        fw = fw_from_slots();              /* (no room for a log line: the screen shows g_emmc_diag) */
#endif
    if (!fw)
        return -1;
    for (uint32_t off = fw; off < 0x100000u; off += 0x800u) {
        if (is_volume_start(0x2000u + off)) {
            *user_start = s_write_floor = 0x2000u + off;
            s_fw_sectors = fw;
            return 0;
        }
    }
    return -2;
}

#endif
