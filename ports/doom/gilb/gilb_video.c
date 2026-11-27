/*
 * gilb_video.c -- the game's 8-bit frame to the panel.
 *
 * The game draws palette indices into a 320 x 170 frame (graphics.h). When a frame is done
 * (I_FinishUpdateBlock -> startDisplayRefresh), its rows go through the palette into RGB565 bands
 * of 8 rows, two of them in turn, for api->blit: blit may still be reading a band when it returns,
 * and the one after it only starts once that transfer is done. Only the rows the game changed are
 * sent: the status bar's 32 rows are skipped when it did not change (st_stuff.c's rowsToUpdate).
 *
 * One frame only (GILB_FRAMES 1): the MG24's second one served its DMA and the screen melt; here
 * the transfer is over when the call returns, and the melt is off (d_main.c).
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include <stdarg.h>
#include <string.h>
#include "gilb_board.h"
#include "graphics.h"

#define BAND 8                       /* rows: 2 x 5 KiB of RGB565 */

static uint8_t s_frame[FRAMEBUFFER_SIZE] __attribute__((section(".fb"), aligned(4)));
static uint16_t s_band[2][SCREEN_WIDTH * BAND] __attribute__((section(".bands"), aligned(4)));
static uint16_t s_pal[256] __attribute__((section(".bands")));   /* the palette, in fast RAM */
static int s_which;

displayData_t displayData;

void gilb_video_setup(void)
{
    displayData.displayFrameBuffer[0] = displayData.displayFrameBuffer[1] = s_frame;
    displayData.workingBuffer = 0;
    displayData.dmaBusy = 0;
    memset(s_frame, 0, sizeof s_frame);
}

/* Every 10 s: the frame rate and the WAD's page misses, in the log. */
static void rate(void)
{
    static uint32_t t0, frames, misses0, ms0;
    uint32_t now = gilb->ms();

    frames++;
    if (!t0) {
        t0 = now;
        misses0 = gilb_wad_misses;
        ms0 = gilb_wad_ms;
    } else if (now - t0 >= 10000u) {
        gilb_logf("doom: %lu.%lu fps, %lu WAD reads taking %lu ms", (unsigned long)(frames * 10000u / (now - t0) / 10u),
                  (unsigned long)(frames * 10000u / (now - t0) % 10u), (unsigned long)(gilb_wad_misses - misses0),
                  (unsigned long)(gilb_wad_ms - ms0));
        gilb_sound_log();
        t0 = now;
        frames = 0;
        misses0 = gilb_wad_misses;
        ms0 = gilb_wad_ms;
    }
}

void startDisplayRefresh(uint8_t bufferNumber, int rowsToUpdate)
{
    rate();
    const uint8_t *src = displayData.displayFrameBuffer[bufferNumber];
    const uint16_t *pal = s_pal;

    if (!displayData.pPalette)
        return;
    memcpy(s_pal, (const void *)displayData.pPalette, sizeof s_pal);   /* (it lives in core B's RAM) */
    if (rowsToUpdate > SCREEN_HEIGHT)
        rowsToUpdate = SCREEN_HEIGHT;
    for (int y0 = 0; y0 < rowsToUpdate; y0 += BAND) {
        int rows = rowsToUpdate - y0 < BAND ? rowsToUpdate - y0 : BAND;
        const uint32_t *s = (const uint32_t *)(src + y0 * SCREEN_WIDTH);
        uint32_t *d = (uint32_t *)s_band[s_which ^= 1];

        for (int i = rows * SCREEN_WIDTH / 4; i; i--) {
            uint32_t p = *s++;

            *d++ = pal[p & 0xFF] | (uint32_t)pal[(p >> 8) & 0xFF] << 16;
            *d++ = pal[(p >> 16) & 0xFF] | (uint32_t)pal[p >> 24] << 16;
        }
        gilb->blit(0, y0, SCREEN_WIDTH, rows, s_band[s_which], SCREEN_WIDTH);
        gilb_sound_tick();
    }
}

/* The MG24's boot text; the game only calls it before the WAD is read. */
void displayPrintln(bool update, const char *format, ...)
{
    (void)update;
    (void)format;
}
