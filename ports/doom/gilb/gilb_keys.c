/*
 * gilb_keys.c -- the player's buttons as Doom's keys (I_StartTic, from the MG24 port's i_video.c).
 *
 * The Echo Mini's keys sit on one resistor ladder: GILB sees one of VOL+, VOL-, PLAY, BACK and MENU
 * at a time (VOL+ with VOL- is the chord), and POWER on a line of its own. So walking is a toggle
 * and the rest can be held while walking (README.md, "Keys"):
 *
 *   in the game      VOL- turn left (held)        VOL+ turn right (held)
 *                    PLAY walk on / off           PLAY held: walk backwards while held
 *                    MENU fire (held)             BACK use (doors, switches); held: the menu
 *                    POWER next weapon            VOL+ and VOL- together: the map
 *   in the menus     VOL- / VOL+ up / down        PLAY select     BACK back
 *                    (VOL- is up, as in GILB's lists built with REVERSE_LISTS=1)
 *                    MENU right (sliders)         POWER left
 *   elsewhere        PLAY or MENU: go on (fire)   BACK: the menu
 *   always           POWER held 2 s: back to GILB (api->poll)
 *
 * A PC keyboard too (API version 4, a USB cable in): tools/gilb_keys.ps1 sends one byte a key
 * change, the Windows key code (VK_*, below 128), bit 7 set for a release; 0x80 (key 0 up) lets go of
 * every key. It sends 0x80 and the keys held 4 times a second too, so a byte lost (its write timed
 * out while the game stalled: device 2026-11-27, a key stuck) mends itself, and the keys held let go
 * when the PC goes quiet for PC_QUIET ms. Those keys can be held
 * together, as on a PC:
 *   W / up arrow   forward             S / down arrow   backwards
 *   left / right   turn                A / D            strafe
 *   Ctrl           fire                Space            use
 *   Shift          run                 Q / E            previous / next weapon
 *   Tab            the map             Esc              the menu (Enter selects)
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include "gilb_board.h"
#include "doomdef.h"
#include "d_event.h"
#include "d_main.h"
#include "global_data.h"
#include "i_video.h"

static uint32_t s_held;              /* keys held down (GK_* bits) */
static uint32_t s_pulse;             /* KEYD_* bits to press for one tic */
static uint32_t s_pulse_next;        /* ...and those to press at the tic after (a press after a release) */
static int s_walk;                   /* 0, 1 = forwards (the toggle), -1 = backwards (PLAY held) */
static uint8_t s_was_level;
static uint32_t s_pc[4];             /* PC keys held (VK codes 0..127), from the USB serial port */
static int s_pc_state;               /* 0 not tried, 1 port open, -1 none (an older GILB, no cable) */
static int s_pc_ready;
static uint32_t s_pc_last;           /* gilb->ms() of the PC's last byte */
#define PC_QUIET 1000u

#define PC(vk) ((s_pc[(vk) >> 5] >> ((vk) & 31)) & 1u)

/* The PC's key changes since the last tic. */
static void pc_keys(void)
{
    uint8_t b[32];
    long n;
    int ready;

    if (s_pc_state == 0) {
        s_pc_state = gilb->version >= 4 && gilb->api_size >= sizeof(gilb_api_t) && gilb->serial_open() == 0 ? 1 : -1;
        if (s_pc_state > 0)
            gilb->log("doom: USB serial port open (tools/gilb_keys.ps1)");
    }
    if (s_pc_state < 0)
        return;
    ready = gilb->serial_ready();
    if (ready != s_pc_ready) {       /* the PC came or went: nothing held */
        s_pc_ready = ready;
        s_pc[0] = s_pc[1] = s_pc[2] = s_pc[3] = 0;
        gilb_logf("doom: PC keyboard %s", ready ? "connected" : "gone");
    }
    while ((n = gilb->serial_read(b, sizeof b)) > 0) {
        s_pc_last = gilb->ms();
        for (long i = 0; i < n; i++) {
            uint32_t vk = b[i] & 0x7Fu, bit = 1u << (vk & 31u);

            if (b[i] == 0x80u)
                s_pc[0] = s_pc[1] = s_pc[2] = s_pc[3] = 0;
            else if (b[i] & 0x80u)
                s_pc[vk >> 5] &= ~bit;
            else
                s_pc[vk >> 5] |= bit;
        }
    }
    if ((s_pc[0] | s_pc[1] | s_pc[2] | s_pc[3]) && gilb->ms() - s_pc_last > PC_QUIET)
        s_pc[0] = s_pc[1] = s_pc[2] = s_pc[3] = 0;
}

/* The PC's keys as Doom's (the same in the menus: up, down, Enter, Esc). */
static uint32_t pc_game_keys(void)
{
    uint32_t k = 0;

    if (PC(0x57) || PC(0x26)) k |= 1u << KEYD_UP;          /* W, up */
    if (PC(0x53) || PC(0x28)) k |= 1u << KEYD_DOWN;        /* S, down */
    if (PC(0x25)) k |= 1u << KEYD_LEFT;
    if (PC(0x27)) k |= 1u << KEYD_RIGHT;
    if (PC(0x41)) k |= 1u << KEYD_SL;                      /* A */
    if (PC(0x44)) k |= 1u << KEYD_SR;                      /* D */
    if (PC(0x11) || PC(0x0D)) k |= 1u << KEYD_FIRE;        /* Ctrl, Enter */
    if (PC(0x20)) k |= 1u << KEYD_USE;                     /* Space */
    if (PC(0x10)) k |= 1u << KEYD_SPEED;                   /* Shift */
    if (PC(0x51)) k |= 1u << KEYD_CHGWDOWN;                /* Q */
    if (PC(0x45)) k |= 1u << KEYD_CHGW;                    /* E */
    if (PC(0x09)) k |= 1u << KEYD_MAP1;                    /* Tab */
    if (PC(0x1B)) k |= 1u << KEYD_MENU;                    /* Esc */
    return k;
}

#define K(k) (1u << (k))

static void key_event(const gilb_key_t *e)
{
    if (e->key == GK_POWER)          /* (a POWER press once ended the game: what GILB saw) */
        gilb_logf("doom: POWER %s at %lu ms", e->gesture == GE_DOWN ? "down" : e->gesture == GE_TAP ? "tap"
                  : e->gesture == GE_HOLD ? "hold" : e->gesture == GE_RELEASE ? "release" : "other",
                  (unsigned long)e->t_ms);
    int menu = _g->menuactive, level = _g->gamestate == GS_LEVEL && !menu;

    switch (e->gesture) {
    case GE_DOWN:
        s_held |= K(e->key);
        if (!level)
            break;
        if (e->key == GK_PLAY)
            s_walk = s_walk ? 0 : 1;
        break;
    case GE_HOLD:
        if (level && e->key == GK_PLAY)
            s_walk = -1;
        else if (e->key == GK_BACK)
            s_pulse |= 1u << KEYD_MENU;  /* the menu (also from the menus: closes it) */
        break;
    case GE_TAP:
    case GE_RELEASE:
        s_held &= ~K(e->key);
        if (e->key == GK_PLAY && s_walk < 0)
            s_walk = 0;
        if (e->gesture != GE_TAP)
            break;
        if (menu) {
            if (e->key == GK_BACK)
                s_pulse |= 1u << KEYD_MENU;
            else if (e->key == GK_POWER)
                s_pulse |= 1u << KEYD_LEFT;
        } else if (level) {
            if (e->key == GK_BACK)
                s_pulse |= 1u << KEYD_USE;
            else if (e->key == GK_POWER)
                s_pulse |= 1u << KEYD_CHGW;
        } else if (e->key == GK_BACK) {
            s_pulse |= 1u << KEYD_MENU;
        }
        break;
    case GE_CHORD:
        s_held &= ~(K(GK_VOLUP) | K(GK_VOLDOWN));
        if (level)
            s_pulse |= 1u << KEYD_MAP1;
        break;
    default:
        break;
    }
}

/* What the game sees as held this tic. */
static uint32_t game_keys(void)
{
    int menu = _g->menuactive, level = _g->gamestate == GS_LEVEL && !menu;
    uint32_t k = s_pulse;

    if (menu) {
        if (s_held & K(GK_VOLDOWN))
            k |= 1u << KEYD_UP;
        if (s_held & K(GK_VOLUP))
            k |= 1u << KEYD_DOWN;
        if (s_held & K(GK_PLAY))
            k |= 1u << KEYD_FIRE;
        if (s_held & K(GK_MENU))
            k |= 1u << KEYD_RIGHT;
    } else if (level) {
        if (s_held & K(GK_VOLDOWN))
            k |= 1u << KEYD_LEFT;
        if (s_held & K(GK_VOLUP))
            k |= 1u << KEYD_RIGHT;
        if (s_held & K(GK_MENU))
            k |= 1u << KEYD_FIRE;
        if (s_walk > 0)
            k |= 1u << KEYD_UP;
        else if (s_walk < 0)
            k |= 1u << KEYD_DOWN;
    } else {
        if (s_held & (K(GK_PLAY) | K(GK_MENU)))
            k |= 1u << KEYD_FIRE;
    }
    return k;
}

uint32_t gilb_keys_state(void)
{
    return s_held;
}

void I_StartTic(void)
{
    static uint32_t old;
    gilb_key_t e;
    uint32_t now, changed;
    event_t ev;
    int level;

    if (gilb->poll())
        gilb_quit_power();
    gilb_sound_tick();
    s_pulse = s_pulse_next;
    s_pulse_next = 0;
    while (gilb->key(&e))
        key_event(&e);
    pc_keys();
    level = _g->gamestate == GS_LEVEL && !_g->menuactive;
    if (!level || !s_was_level)
        s_walk = 0;                  /* a menu or a new level stops the walk */
    s_was_level = (uint8_t)level;

    now = game_keys() | pc_game_keys();
    /* A pulse on a key the game already sees held: release it now, press it next tic. */
    if (s_pulse & old) {
        s_pulse_next |= s_pulse & old;
        now &= ~(s_pulse & old);
    }
    changed = old ^ now;
    ev.type = ev_keydown;
    for (int i = 0; i < NUMKEYS; i++)
        if ((changed >> i) & (now >> i) & 1u) {
            ev.data1 = i;
            D_PostEvent(&ev);
        }
    ev.type = ev_keyup;
    for (int i = 0; i < NUMKEYS; i++)
        if ((changed >> i) & 1u & ~(now >> i)) {
            ev.data1 = i;
            D_PostEvent(&ev);
        }
    old = now;
}
