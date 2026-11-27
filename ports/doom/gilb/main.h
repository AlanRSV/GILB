/*
 * main.h -- the game's configuration on GILB (the SNOWSKY Echo Mini).
 *
 * Replaces the MG24 port's src/main.h (Nicola Wrachien, GPLv2 or later): the same switches, set
 * for this player. The hardware parts (timers, SPI, DMA) are gone; gilb_board.h has what replaced
 * them.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#ifndef MAIN_H
#define MAIN_H
#include <stdbool.h>
#include "macros.h"
#include "gilb_board.h"

#define DISABLE_CACHING_TEXTURE_TO_FLASH  0
#define TEST_DISABLE_ASYNCH_LOAD          0
#define FAST_CPU_SMALL_FLASH              1     /* smaller sine/tangent tables: RAM is short */
#define CORRECT_TABLE_ERROR               1
#define CACHE_ALL_COLORMAP_TO_RAM         0
#define DEMO_SUPPORT                      1
#define DOOM2SUPPORT                      1
#define HAS_NETWORK                       0     /* no Bluetooth game */
#define DEBUG_SETUP                       0
#define SHOW_FPS_IN_HUD                   true
#define SHOW_FPS_IN_AMMO                  false
#define TIME_DEMO                         NULL
#define NO_DEMO_LOOP                      true
#define START_MAP                         1
#define DEBUG_OUT_PRINTF                  0     /* printf() goes nowhere; I_Error notes go to GILB's log */
#define ENABLE_MUSIC                      0     /* silent first build: no OPL2 */
#define TICK_TIMER_FREQUENCY_HZ           (1000000UL)
#define TIMER_TICKS_PER_MILLISECOND       (TICK_TIMER_FREQUENCY_HZ / 1000UL)
#endif
