/*
 * graphics.h -- GILB: the frame the game draws into, 8 bits a pixel (a palette index).
 *
 * The MG24 port kept two whole frames in displayData_t and sent one by DMA while the game drew the
 * other. Here gilb_video.c turns the frame into RGB565 bands for api->blit as soon as it is done,
 * so the frames are pointers (gilb_board.c sets them): two frames only when there is room for the
 * screen melt, else both point at the same one (GILB_FRAMES).
 */
#ifndef SRC_GRAPHICS_H_
#define SRC_GRAPHICS_H_
#include <stdbool.h>
#include <stdint.h>
#include "display.h"
#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
typedef struct
{
    volatile uint16_t *pPalette;
    uint8_t *displayFrameBuffer[2];
    uint8_t workingBuffer;
    volatile uint8_t dmaBusy;        /* always 0: the transfer is over when startDisplayRefresh returns */
} displayData_t;
extern displayData_t displayData;
void startDisplayRefresh(uint8_t bufferNumber, int rowsToUpdate);
void displayPrintln(bool update, const char *format, ...);
#endif
