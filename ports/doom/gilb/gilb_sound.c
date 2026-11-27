/*
 * gilb_sound.c -- Doom's sound effects through GILB's sound API (version 5).
 *
 * The mixer is the MG24 port's (src/audio.c, Nicola Wrachien): up to MAX_CHANNELS effects, 8-bit
 * unsigned samples read straight from the WAD (11025 Hz, or 22050 Hz read every other byte), mixed
 * in stereo at the volumes i_audio.c sets. Here it writes GILB's ring (api->sound_open): RING frames
 * of two int32 words (L, R), which the audio DMA plays in a loop.
 *
 * At 44100 Hz, four frames a sample (linearly between them): the player's DAC (CS43131) takes 32 kHz
 * and up, so 11025 Hz played nothing (device 2026-11-26, the DMA moving, the ring full of sound).
 *
 * Each update mixes from DELAY frames ahead of the DMA up to just behind it. GILB's DMA position
 * wraps every RING frames (46 ms), less than a slow frame, so the mixer keeps an absolute frame count
 * (the DMA position plus the wraps the clock says happened) and each channel's start in it: an update
 * after a long frame picks the effects up where they really are. Updates come from many places
 * (gilb_sound_tick: the keys, the renderer's NetUpdate calls, the end of each storage read, the
 * panel's bands), at most every TICK_MS. A mix saves and restores the WAD reader's position: it runs
 * inside other reads.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include <string.h>
#include "gilb_board.h"
#include "audio.h"
#include "extMemory.h"
#include "doomdef.h"
#include "w_wad.h"
#include "global_data.h"

#define UP         4u                     /* output frames a sample: 11025 Hz -> 44100 Hz */
#define RATE       (11025u * UP)
#define BLOCKS     16u
#define BLOCK      128u
#define RING       (BLOCKS * BLOCK)       /* 2048 frames, 46 ms (GILB_SOUND_RING_BYTES) */
#define DELAY      256u                   /* frames ahead of the DMA where a mix starts (5.8 ms) */
#define GUARD      32u                    /* ...and stops, behind it */
#define SPAN       (RING - DELAY - GUARD) /* 1760 frames, 40 ms */
#define TICK_MS    10u
#define VOL_SHIFT  14                     /* (sample * volume) to the top bits: +-2^28 a channel */

_Static_assert(RING * 8u == GILB_SOUND_RING_BYTES, "the ring's size (gilb_board.h)");

soundChannel_t soundChannels[MAX_CHANNELS];

int32_t *gilb_sound_ring;                 /* RING x 2 words in core A's RAM (gilb_main.c: AHI) */
static int s_on;
static int s_busy;
static uint32_t s_abs;                    /* absolute frame of the DMA position at the last update */
static uint32_t s_pos;                    /* ...and that position in the ring */
static uint32_t s_ms;                     /* ...and when */
static uint32_t s_start[MAX_CHANNELS];    /* each channel's first frame, absolute */
static uint32_t s_tick;
/* For the log (gilb_sound_log): mixes, the ones with an effect in, the loudest sample written,
   effects started, the DMA positions seen. */
static uint32_t s_mixes, s_mixed, s_peak, s_starts, s_pos_first, s_pos_moves;

void initAudio(void)
{
    s_on = 0;
    if (gilb->version < 5 || gilb->api_size < sizeof(gilb_api_t) || !gilb_sound_ring)
        return;
    if (gilb->sound_open(RATE, gilb_sound_ring, BLOCK, BLOCKS) != 0) {
        gilb->log("doom: no sound (the DAC refused)");
        return;
    }
    s_pos = gilb->sound_pos();
    s_abs = s_pos;
    s_ms = gilb->ms();
    s_on = 1;
}

void muteSound(void)
{
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        soundChannels[ch].sfxIdx = 0;
        soundChannels[ch].volumeLeft = soundChannels[ch].volumeRight = 0;
    }
}

/* Where the DMA is, as an absolute frame. */
static uint32_t dma_abs(void)
{
    uint32_t pos = gilb->sound_pos(), now = gilb->ms();
    uint32_t delta = (pos - s_pos) & (RING - 1u);
    uint32_t expect = (now - s_ms) * (RATE / 100u) / 10u;   /* what the clock says passed */

    if (expect > delta + RING / 2u)                         /* the ring went round meanwhile */
        delta += (expect - delta + RING / 2u) / RING * RING;
    s_abs += delta;
    s_pos = pos;
    s_ms = now;
    return s_abs;
}

static inline int32_t sat_add(int32_t a, int32_t b)
{
    int32_t r;

    if (__builtin_add_overflow(a, b, &r))
        r = a > 0 ? INT32_MAX : INT32_MIN;
    return r;
}

void updateSound(void)
{
    uint32_t first, at;
    int32_t *ring = gilb_sound_ring;
    uint32_t caller_at = currentSpiAddress;  /* (a mix may run inside another read: the level set-up's
                                                sequential reads went astray, device 2026-11-26) */

    if (!s_on || s_busy)
        return;
    s_busy = 1;
    s_tick = gilb->ms();
    first = dma_abs() + DELAY;                              /* the first frame this update writes */
    s_mixes++;
    if (s_pos != s_pos_first)
        s_pos_moves++;
    s_pos_first = s_pos;
    at = first & (RING - 1u);
    for (uint32_t k = 0, i = at; k < SPAN; k++, i = (i + 1u) & (RING - 1u))
        ring[2u * i] = ring[2u * i + 1u] = 0;

    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        soundChannel_t *c = &soundChannels[ch];
        const soundLumpData_t *sl;
        uint32_t played, inc, s0, nsamples, frames, len;
        uint8_t tmp[2u * (SPAN / UP + 4u)];

        if (!c->sfxIdx || (!c->volumeLeft && !c->volumeRight))
            continue;
        sl = &p_wad_immutable_flash_data->soundLumps[c->sfxIdx];
        if (!isOnExternalFlash(sl->lumpAddress))
            continue;
        if (c->lastAudioBufferIdx == 0xFFFF) {              /* new (I_StartSound): it starts here */
            s_start[ch] = first;
            c->lastAudioBufferIdx = 0;
            s_starts++;
        }
        played = first - s_start[ch];                       /* output frames of it already behind us */
        inc = sl->increment ? (uint32_t)sl->increment : 1u;
        len = (uint32_t)sl->length / inc;                   /* its samples (at 11025 Hz) */
        s0 = played / UP;                                   /* the sample this mix starts in */
        if ((int32_t)played < 0 || s0 >= len) {             /* over */
            c->sfxIdx = 0;
            c->volumeLeft = c->volumeRight = 0;
            continue;
        }
        frames = (len - s0) * UP - played % UP;             /* output frames left of it */
        if (frames > SPAN)
            frames = SPAN;
        nsamples = (played % UP + frames + UP - 1u) / UP + 1u;   /* (and the next, to lean towards) */
        if (s0 + nsamples > len)
            nsamples = len - s0;
        extMemSetCurrentAddress((uint32_t)sl->lumpAddress + DMX_DATA_SOUND_OFFSET + s0 * inc);
        extMemGetDataFromCurrentAddress(tmp, nsamples * inc);
        {
            int32_t vl = c->volumeLeft, vr = c->volumeRight, vmax = vl > vr ? vl : vr;
            uint32_t f = played % UP;                       /* where between two samples */
            const uint8_t *p = tmp;

            for (uint32_t k = 0, i = at; k < frames; k++, i = (i + 1u) & (RING - 1u)) {
                int32_t a = (int32_t)p[0] - 128;
                int32_t b = p + inc < tmp + nsamples * inc ? (int32_t)p[inc] - 128 : a;
                int32_t s4 = a * (int32_t)(UP - f) + b * (int32_t)f;   /* the sample x UP */

                ring[2u * i] = sat_add(ring[2u * i], (s4 * vl) << (VOL_SHIFT - 2));
                ring[2u * i + 1u] = sat_add(ring[2u * i + 1u], (s4 * vr) << (VOL_SHIFT - 2));
                if ((uint32_t)(s4 < 0 ? -s4 : s4) / UP * (uint32_t)vmax > s_peak)
                    s_peak = (uint32_t)(s4 < 0 ? -s4 : s4) / UP * (uint32_t)vmax;
                if (++f == UP) {
                    f = 0;
                    p += inc;
                }
            }
            s_mixed++;
        }
    }
    currentSpiAddress = caller_at;
    s_busy = 0;
}

/* A line in the log (gilb_video.c, every 10 s) when the sound looks stuck: effects started but none
   mixed, or the DMA standing still. Then the counts start again. */
void gilb_sound_log(void)
{
    if (!s_on || (s_mixes && s_pos_moves * 2u >= s_mixes && (s_starts == 0 || s_mixed))) {
        s_mixes = s_mixed = s_peak = s_starts = s_pos_moves = 0;
        return;                      /* (all as it should be: nothing to say) */
    }
    gilb_logf("doom: sound: %lu mixes (DMA moved in %lu), %lu with effects, %lu started, peak %lu/16256, "
              "sfx volume %u", (unsigned long)s_mixes, (unsigned long)s_pos_moves, (unsigned long)s_mixed,
              (unsigned long)s_starts, (unsigned long)s_peak, (unsigned)_g->snd_SfxVolume);
    s_mixes = s_mixed = s_peak = s_starts = s_pos_moves = 0;
}

/* From anywhere, often: a mix when TICK_MS have passed since the last. */
void gilb_sound_tick(void)
{
    if (s_on && !s_busy && gilb->ms() - s_tick >= TICK_MS)
        updateSound();
}
