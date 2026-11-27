/*
 * render.c -- Doom's music, rendered once on the PC for GILB (ports/doom/README.md, "Music").
 *
 *     make -C ports/doom/tools/music
 *     ports/doom/tools/music/doom_music DOOM1.WAD OUTDIR
 *
 * Plays each song of the WAD (the D_* lumps, MUS scores) through the MG24 port's own OPL2 synthesis
 * and MUS player (../../source/opl2.c, mus.c, Nicola Wrachien, GPLv2 or later), with the WAD's
 * instruments (GENMIDI), so it sounds as the port does. Each song goes to OUTDIR/D_xxxx.PCM: one
 * pass of its score, 16-bit signed little-endian mono at 11025 Hz, at full music volume (the game
 * applies its own). On the player, Doom streams them from GILB/APPS/DOOMMUS (gilb/gilb_sound.c).
 *
 * Works on the shareware DOOM1.WAD or the converted DOOM.WAD alike: both keep the songs and
 * GENMIDI as they are.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mus.h"
#include "opl2.h"
#include "w_wad.h"
#include "global_data.h"

#define RATE 11025u

host_globals_t g_host;
static uint32_t s_addrs[2];
static host_wad_t s_wad;
host_wad_t *p_wad_immutable_flash_data = &s_wad;
const uint8_t *g_wad;
uint32_t g_wad_len, g_at;

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* The score's length in MUS ticks (140 a second): the delays up to its end. */
static uint32_t score_ticks(const uint8_t *s, const uint8_t *end)
{
    uint32_t ticks = 0;

    while (s < end) {
        uint8_t d = *s++, ev = d & 0x70;

        switch (ev) {
        case 0x00: s += 1; break;                          /* release */
        case 0x10: if (*s++ & 0x80) s++; break;            /* press (+ volume) */
        case 0x20: s += 1; break;                          /* pitch wheel */
        case 0x30: s += 1; break;                          /* system */
        case 0x40: s += 2; break;                          /* controller */
        case 0x60: return ticks;                           /* the end */
        default: return ticks;
        }
        if (d & 0x80) {
            uint32_t t = 0;
            uint8_t b;

            do {
                b = *s++;
                t = t * 128u + (b & 0x7Fu);
            } while ((b & 0x80) && s < end);
            ticks += t;
        }
    }
    return ticks;
}

int main(int argc, char **argv)
{
    FILE *f;
    long len;
    uint8_t *w;
    uint32_t n, dir, genmidi = 0;
    int songs = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s DOOM1.WAD OUTDIR\n", argv[0]);
        return 2;
    }
    if (!(f = fopen(argv[1], "rb")) || fseek(f, 0, SEEK_END) || (len = ftell(f)) < 12) {
        fprintf(stderr, "can't read %s\n", argv[1]);
        return 1;
    }
    w = malloc((size_t)len);
    rewind(f);
    if (fread(w, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "can't read %s\n", argv[1]);
        return 1;
    }
    fclose(f);
    g_wad = w;
    g_wad_len = (uint32_t)len;
    n = u32(w + 4);
    dir = u32(w + 8);
    for (uint32_t i = 0; i < n; i++)
        if (!strncmp((const char *)w + dir + 16 * i + 8, "GENMIDI", 8))
            genmidi = u32(w + dir + 16 * i);
    if (!genmidi) {
        fprintf(stderr, "no GENMIDI in %s\n", argv[1]);
        return 1;
    }
    s_wad.instruments = (const genmidi_instr_t *)(w + genmidi + GENMIDI_HEADER_SIZE);
    s_wad.musicAddrs = s_addrs;
    OPL2_Reset(RATE);

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *e = w + dir + 16 * i;
        uint32_t pos = u32(e), size = u32(e + 4), ticks, total;
        char name[9] = { 0 }, path[512];
        int32_t buf[1024];
        int16_t out[1024];

        memcpy(name, e + 8, 8);
        if (strncmp(name, "D_", 2) || size < 16 || memcmp(w + pos, "MUS\x1A", 4))
            continue;
        ticks = score_ticks(w + pos + u16(w + pos + 6), w + pos + size);
        total = (uint32_t)((uint64_t)ticks * RATE / 140u);
        snprintf(path, sizeof path, "%s/%s.PCM", argv[2], name);
        if (!(f = fopen(path, "wb"))) {
            fprintf(stderr, "can't write %s\n", path);
            return 1;
        }
        s_addrs[1] = pos + u16(w + pos + 6);              /* the score, after the MUS header */
        g_host.mus_playing = 1;
        setMusic(1);
        OPL_InitRegisters();
        initMusic();
        I_OPL_SetMusicVolume(127);
        for (uint32_t done = 0; done < total; ) {
            uint32_t k = total - done < 1024u ? total - done : 1024u;

            generateMusicOutput(buf, k);
            for (uint32_t j = 0; j < k; j++)
                out[j] = (int16_t)(buf[j] > 32767 ? 32767 : buf[j] < -32768 ? -32768 : buf[j]);
            fwrite(out, 2, k, f);
            done += k;
        }
        fclose(f);
        printf("%-8s %4u s  %s\n", name, (unsigned)(total / RATE), path);
        songs++;
    }
    printf("%d songs\n", songs);
    return 0;
}
