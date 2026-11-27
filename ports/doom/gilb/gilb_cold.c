/*
 * gilb_cold.c -- what the MG24 board layer did that GILB does without (cold code, core B's RAM):
 * the music driver (no OPL2 yet; the sound effects are gilb_sound.c's) and the Bluetooth game's tic
 * exchange.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include "gilb_board.h"
#include "audio.h"
#include "doomdef.h"
#include "d_net.h"
#include "global_data.h"
#include "mus.h"

void initMusic(void) { }
void OPL_InitRegisters(void) { }
void setMusic(uint8_t id_num) { (void)id_num; }
void I_OPL_SetMusicVolume(int volume) { (void)volume; }

/* The MG24 port calls this from its renderer too, to keep the network fed; alone, it builds the
 * local player's tic commands (d_client.c's !HAS_NETWORK path). That also reads the keys and keeps
 * GILB's poll going during a long frame. */
void NetUpdate(void)
{
    gilb_sound_tick();
    if (!_g->gameStarted)
        return;
    D_BuildNewTiccmds();
}
