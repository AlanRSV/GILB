/*
 * gilb_board.h -- what the Doom port gets from GILB instead of the MG24 board.
 *
 * The game (../source, ../include) is Nicola Wrachien's MG24 Doom, itself doomhack's GBA Doom with
 * Kippykip's additions, from PrBoom (GPLv2 or later). Its board layer (DoomMG24BLE/src: the SPI
 * display, the keys, the SPI flash, the PWM sound, Bluetooth) is replaced by this folder, over
 * GILB's program API (sdk/include/gilb_app.h). ports/doom/README.md has the whole story.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#ifndef GILB_BOARD_H
#define GILB_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include "gilb_app.h"

extern const gilb_api_t *gilb;            /* gilb_main.c */

/* Interrupts: GILB's SysTick samples the keys; the game only masks them around its stack switch. */
static inline void __disable_irq(void) { __asm volatile("cpsid i" ::: "memory"); }
static inline void __enable_irq(void) { __asm volatile("cpsie i" ::: "memory"); }
static inline uint32_t __get_PRIMASK(void) { uint32_t r; __asm volatile("mrs %0, primask" : "=r"(r)); return r; }
static inline void __set_PRIMASK(uint32_t v) { __asm volatile("msr primask, %0" :: "r"(v) : "memory"); }

/* The MG24's "let the radio stack run": here, GILB's own work (api->poll). */
void sl_system_process_action(void);

/* The player's keys, as the game's KEYD_* bits (gilb_keys.c). */
uint32_t gilb_keys_state(void);

/* Memory the build places (doom.ld, gilb_main.c): the "flash" cache, the WAD file's read cache. */
extern uint8_t *gilb_cache_base;          /* where FLASH_ADDRESS points */
extern uint32_t gilb_cache_size;
extern uint8_t *gilb_wadcache_base;       /* gilb_wad.c's pages */
extern uint32_t gilb_wadcache_size;
#define GILB_WAD_PAGE        1024u   /* a column read wastes less of a page */
#define GILB_WAD_ZONE        (32u * 1024u)   /* the WAD cache's share of the zone (8 KiB more since the sound ring took AHI's) */
extern uint32_t gilb_wad_misses, gilb_wad_ms;
extern int32_t *gilb_sound_ring;          /* gilb_sound.c's ring, 16 KiB in AHI (gilb_main.c) */
#define GILB_SOUND_RING_BYTES (2048u * 8u)   /* 2048 frames at 44100 Hz: 46 ms */
void gilb_sound_tick(void);
void gilb_sound_log(void);

void gilb_wad_close(void);
/* d_main.c's loop: this frame's sound update and game logic (ms, and the tics it ran), and when its
 * drawing began; gilb_video.c logs the worst frame's in its 10 s line. */
typedef struct { uint32_t snd, logic, tics, draw0; } gilb_prof_t;
extern gilb_prof_t gilb_prof;
int gilb_zone_squeeze(void);              /* gilb_wad.c: the WAD cache gives the zone its share back */
void gilb_zone_regrow(void);              /* ...and takes it again at a level's start, if there is room */
extern int gilb_level_must_fit;           /* w_wad.c: a level lump that doesn't fit the cache stops */
extern uint32_t currentImmutableFlashOffset, currentLevelFlashOffset;   /* w_wad.c */
void gilb_quit(int rc) __attribute__((noreturn));   /* back to GILB (longjmp) */
void gilb_quit_power(void) __attribute__((noreturn));
void gilb_fatal(const char *why);         /* a message, then back to GILB */
void gilb_logf(const char *fmt, ...);
void I_Error(const char *error, ...);   /* a note in the log; the game goes on (lprintf.h) */
void gilb_error(const char *error, ...) __attribute__((noreturn));   /* the message, then back to GILB */
/* Where the MG24 port stopped dead (while (1);), GILB says where and goes back. */
#define GILB_BLOCKED() gilb_error("stopped at %s:%d", __FILE__, __LINE__)

/* Code and data placement (doom.ld). HOT: core A's RAM; COLD: core B's (runs 4.8x slower). */
#define GILB_COLD __attribute__((section(".cold.text")))
#endif
