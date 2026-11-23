/*
 * rg_read.c -- ReplayGain: a track's gains from its tags, for core B (audio_shared.h as_track_t rg).
 *
 * In the npx overlay (firmware_ap.ld) with its tag reader (services/tags_rg.c); audio.c reaches it
 * through audio_rg_read (startup/ap_ovl.c) as it queues each track, and the simulator as it starts
 * one. Core B multiplies the decoded samples by the factor the setting picks (g_cfg.rg: off, track,
 * album), before the EQ.
 */
#include <string.h>

#include "services/dirlist.h"
#include "services/fs.h"
#include "services/log.h"
#define TAGS_RG 1
#include "services/tags.h"
#include "ui/np_extra.h"

static uint32_t rd_at(void *u, uint32_t off, void *buf, uint32_t n)
{
    long got;

    if (fs_seek((fs_file_t *)u, (long)off) < 0)
        return 0;
    got = fs_read((fs_file_t *)u, buf, n);
    return got > 0 ? (uint32_t)got : 0u;
}

/* 10^(cdb / 2000) (cdb: 0.01 dB, -40..+18 dB) as a Q4.28 factor: 2^(cdb * log2(10) / 2000), the
 * fraction's power from a cubic (within 0.002 dB). */
static int32_t rg_factor(int32_t cdb)
{
    int32_t e, ip;
    uint32_t f, m;

    cdb = cdb > 1800 ? 1800 : cdb < -4000 ? -4000 : cdb;
    e = cdb * 27866;                         /* log2 of the factor, Q24 */
    ip = e >> 24;                            /* (arithmetic: floor) */
    f = (uint32_t)e & 0xFFFFFFu;
    m = (uint32_t)(((uint64_t)f * 1298232u) >> 24) + 3807423u;
    m = (uint32_t)(((uint64_t)f * m) >> 24) + 11667380u;
    m = (uint32_t)(((uint64_t)f * m) >> 24) + (1u << 24);   /* 2^f, Q24 */
    return (int32_t)(ip >= -4 ? m << (ip + 4) : m >> (-4 - ip));
}

void rg_read(const fs_ref_t *ref, int kind, int32_t g[2])
{
    tag_info_t t;
    fs_file_t *f;

    g[0] = g[1] = 0;
    if (kind == DL_WMA || kind == DL_DSF || kind == DL_DFF || !(f = fs_open_ref(ref)))
        return;
    tags_read(rd_at, f, (uint32_t)fs_file_size(f),
              kind == DL_FLAC ? TAGS_FLAC : kind == DL_WAV ? TAGS_WAV : kind == DL_M4A ? TAGS_MP4 :
              kind == DL_OGG || kind == DL_OPUS ? TAGS_OGG : TAGS_MP3, &t);
    fs_close(f);
    for (int i = 0; i < 2; i++) {
        int j = t.rg & 1u << i ? i : 0;      /* (no album gain: the track's) */

        if (!(t.rg & 1u << j))
            continue;
        g[i] = rg_factor(t.rg_db[j]);
        if (t.rg & 4u << j && t.rg_pk[j]) {  /* the tagged peak at most full scale */
            uint64_t lim = ((uint64_t)10000u << 28) / t.rg_pk[j];

            if ((uint64_t)g[i] > lim)
                g[i] = (int32_t)lim;
        }
    }
    if (t.rg & 3u)
        log_printf("rg: track %d album %d (0.01 dB), peaks %u %u: x%08lx x%08lx", t.rg_db[0],
                   t.rg & 2u ? t.rg_db[1] : t.rg_db[0], t.rg_pk[0], t.rg_pk[1], (unsigned long)g[0],
                   (unsigned long)g[1]);
}
