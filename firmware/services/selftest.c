/*
 * selftest.c -- Settings > System > Self-test (the dev overlay: startup/ap_ovl.c). For a player
 * just flashed, bought used or repaired: each part checked in one guided run, a summary at the
 * end and one log line per part ("selftest: ...").
 *
 *   Keys       press each of the six when asked (8 s each)
 *   Audio      4 s of tone in the left ear, 2 s of silence, 4 s in the right, through the jack; the listener answers
 *   Storage    the internal volume: 512 KB written, read back, compared, removed; its bus width
 *   SD card    the same on the card (skipped without one)
 *   Clock      the time of day known, and running
 *   Battery    a sane voltage; charging seen with a cable
 *   Bluetooth  the module powered and its hello (CV1x) heard; off again if it was off
 *   USB        the cable seen or not (information)
 *
 * Playback stops first. The DAC is brought up for the tone and handed back as after a USB session
 * (usb_dac_rail_cut: the next play brings it up again). Its own screen helpers: those of ap_ui.c
 * are static, and its strings stay in this overlay (main memory is nearly full).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fs.h"
#include "input.h"
#include "log.h"
#include "power.h"
#include "scratch.h"
#include "sdcard.h"
#include "wallclock.h"
#include "watchdog.h"
#include "drivers/bt_jieli.h"
#include "drivers/clock.h"
#include "drivers/dac_cs43131.h"
#include "drivers/display.h"
#include "drivers/emmc.h"
#include "drivers/panel_io.h"
#include "drivers/usb_msc.h"
#include "player/player.h"

void ap_pump(void);                      /* startup/ap_ui.c: the music, storage queue, log, BT */

enum { V_INFO, V_PASS, V_FAIL, V_SKIP };
enum { T_KEYS, T_AUDIO, T_INT, T_SD, T_CLOCK, T_BATT, T_BT, T_USB, T_N };
static const char *const k_name[T_N] = { "Keys", "Audio", "Storage", "SD card", "Clock", "Battery",
                                         "Bluetooth", "USB" };
static struct { uint8_t v; char d[30]; } R[T_N];

static void st_line(int row, const char *s, uint16_t fg)
{
    display_draw_text(8, 8 + row * 14, s, fg, 0x0000);
}

static void st_screen(const char *title, const char *l2, const char *l3)
{
    display_fill_rect(0, 0, DISPLAY_W - 1, DISPLAY_H - 1, 0x0000);
    st_line(0, title, 0xFFFF);
    if (l2)
        st_line(2, l2, 0xFFFF);
    if (l3)
        st_line(3, l3, 0xC618);
    display_flush_rows(0, DISPLAY_H - 1);
}

/* The next key (not a repeat), or KEY_NONE after ms; the loop's work goes on meanwhile. */
static int st_key(uint32_t ms)
{
    uint32_t t0 = time_ms();
    input_event_t ev;

    while (time_ms() - t0 < ms) {
        input_scan(keys_adc(), keys_power());
        while (input_poll(&ev) > 0)
            if (ev.adc_value != INPUT_ADC_REPEAT && ev.type != KEY_NONE) {
                power_activity();        /* (the screen-off timer: a key, as in the main loop) */
                return ev.type;
            }
        ap_pump();
        cpu_wait_tick();
    }
    return KEY_NONE;
}

static void set(int t, int v, const char *d)
{
    R[t].v = (uint8_t)v;
    snprintf(R[t].d, sizeof R[t].d, "%s", d);
    log_printf("selftest: %s %s: %s", k_name[t], v == V_PASS ? "ok" : v == V_FAIL ? "FAIL" : v == V_SKIP ? "skipped" : "-", d);
}

static void t_keys(void)
{
    static const uint8_t want[6] = { KEY_UP, KEY_DOWN, KEY_SELECT, KEY_BACK, KEY_MENU, KEY_POWER };
    static const char *const nm[6] = { "VOL+", "VOL-", "PLAY", "BACK", "MENU", "POWER" };
    char b[30], miss[26] = "";
    int ok = 0;

    while (keys_power())
        cpu_wait_tick();
    input_mock_clear();
    for (int i = 0; i < 6; i++) {
        uint32_t t0 = time_ms();
        int k = KEY_NONE;

        snprintf(b, sizeof b, "Press %s", nm[i]);
        st_screen("Self-test 1/6: keys", b, "(8 s; it moves on by itself)");
        while (k != want[i] && time_ms() - t0 < 8000u)
            k = st_key(8000u - (time_ms() - t0));
        if (k == want[i])
            ok++;
        else
            snprintf(miss + strlen(miss), sizeof miss - strlen(miss), " %s", nm[i]);
    }
    if (ok == 6)
        set(T_KEYS, V_PASS, "all 6");
    else {
        snprintf(b, sizeof b, "no%s", miss);
        set(T_KEYS, V_FAIL, b);
    }
}

/* ~690 Hz (a 64-sample triangle at 44.1 kHz) for ms on one side (ch 0 left, 1 right; 2: silence, the
 * DAC kept running so neither side clicks). Fed the watchdog: a tone outlasts its 4.5 s. */
static void tone(int ch, uint32_t ms)
{
    int16_t *c = (int16_t *)g_scratch;
    uint32_t n = 0, frames = 44100u * ms / 1000u;

    dac_i2s_start();
    while (n < frames) {
        for (int i = 0; i < 256; i++) {
            int p = (int)((n + (uint32_t)i) & 63u), s = (p < 32 ? p - 16 : 48 - p) * 400;

            c[2 * i] = (int16_t)(ch == 0 ? s : 0);
            c[2 * i + 1] = (int16_t)(ch == 1 ? s : 0);
        }
        if (dac_i2s_feed(c, 256) != DAC_OK)
            break;
        n += 256u;
        watchdog_feed();
    }
    dac_i2s_stop();
}

static void t_audio(void)
{
    int r;

    st_screen("Self-test 2/6: audio", "Headphones in the jack, then PLAY", "BACK: skip");
    r = st_key(30000u);
    if (r != KEY_SELECT) {
        set(T_AUDIO, V_SKIP, "not run");
        return;
    }
    r = dac_init(44100);
    if (r != DAC_OK) {
        char b[30];

        snprintf(b, sizeof b, "DAC bring-up error %d", r);
        set(T_AUDIO, V_FAIL, b);
        dac_output_off();
        usb_dac_rail_cut();
        return;
    }
    st_screen("Self-test 2/6: audio", "A tone in the LEFT ear...", NULL);
    tone(0, 4000u);                      /* (4 s a side, 2 s of silence between: each side heard apart) */
    st_screen("Self-test 2/6: audio", "(a pause)", NULL);
    tone(2, 2000u);
    st_screen("Self-test 2/6: audio", "...then in the RIGHT ear", NULL);
    tone(1, 4000u);
    dac_output_off();
    usb_dac_rail_cut();                  /* (the next play brings the DAC up as it wants it) */
    st_screen("Self-test 2/6: audio", "Heard left, then right?", "PLAY yes   BACK no");
    r = st_key(30000u);
    set(T_AUDIO, r == KEY_SELECT ? V_PASS : V_FAIL, r == KEY_SELECT ? "left and right heard" : "not heard right");
}

/* 512 KB to dir/SELFTEST.BIN, read back and compared, removed. 0, or the step that failed. */
static const char *disk(const char *dir, uint32_t *tw, uint32_t *tr)
{
    char p[40];
    uint8_t *b = (uint8_t *)g_scratch;
    const uint32_t chunk = sizeof g_scratch, total = 512u * 1024u;
    fs_file_t *f;
    const char *bad = NULL;
    uint32_t t0;

    (void)fs_mkdir(dir);                 /* (there already: FS_ERR_EXIST) */
    snprintf(p, sizeof p, "%s/SELFTEST.BIN", dir);
    t0 = time_ms();
    if (!(f = fs_open_write(p)))
        return "open";
    for (uint32_t at = 0; at < total && !bad; at += chunk) {
        for (uint32_t i = 0; i < chunk; i++)
            b[i] = (uint8_t)((at + i) * 7u + ((at + i) >> 9));
        if (fs_write(f, b, chunk) != (long)chunk)
            bad = "write";
    }
    if (fs_close(f) && !bad)
        bad = "close";
    *tw = time_ms() - t0;
    t0 = time_ms();
    if (!bad && !(f = fs_open_read(p)))
        bad = "reopen";
    for (uint32_t at = 0; at < total && !bad; at += chunk) {
        if (fs_read(f, b, chunk) != (long)chunk) {
            bad = "read";
            break;
        }
        for (uint32_t i = 0; i < chunk && !bad; i++)
            if (b[i] != (uint8_t)((at + i) * 7u + ((at + i) >> 9)))
                bad = "compare";
    }
    if (f && !bad)
        fs_close(f);
    *tr = time_ms() - t0;
    if (fs_remove(p) && !bad)
        bad = "remove";
    return bad;
}

static void t_disk(int t, const char *dir)
{
    uint32_t tw = 0, tr = 0;
    const char *bad;
    char b[30];

    st_screen(t == T_INT ? "Self-test 3/6: storage" : "Self-test 4/6: SD card", "Writing and reading 512 KB...", NULL);
    bad = disk(dir, &tw, &tr);
    if (bad) {
        snprintf(b, sizeof b, "%s failed", bad);
        set(t, V_FAIL, b);
        return;
    }
    {
        unsigned w = 5243u / (tw ? tw : 1u), r = 5243u / (tr ? tr : 1u);   /* MB/s x 10 (512 KiB) */

        w = w > 999u ? 999u : w;
        r = r > 999u ? 999u : r;
        snprintf(b, sizeof b, "%s%sw/r %u.%u/%u.%u MB/s", t == T_INT ? (emmc_width() == 8 ? "8" : emmc_width() == 4 ? "4" : "1") : "",
                 t == T_INT ? "-bit, " : "", w / 10u, w % 10u, r / 10u, r % 10u);
    }
    set(t, t == T_INT && emmc_width() < 4 ? V_FAIL : V_PASS, b);   /* (1 bit: the fallback, slow) */
}

static void t_clock(void)
{
    uint32_t a, b2, t0 = time_ms();
    char b[30];

    st_screen("Self-test 5/6: clock", "Checking that it runs...", NULL);
    if (wallclock_now(&a)) {
        set(T_CLOCK, V_FAIL, "not set (Echo Library sets it)");
        return;
    }
    while (time_ms() - t0 < 2100u)
        ap_pump(), cpu_wait_tick();
    wallclock_now(&b2);
    wallclock_text(b, sizeof b);
    set(T_CLOCK, b2 - a >= 1u && b2 - a <= 3u ? V_PASS : V_FAIL, b);
}

static void t_battery(void)
{
    const power_battery_t *p = power_battery();
    char b[30];

    snprintf(b, sizeof b, "%u.%02u V, %u %%%s", p->mv / 1000u, p->mv % 1000u / 10u, p->pct10 / 10u,
             p->vbus ? (p->full ? ", full" : ", charging") : "");
    set(T_BATT, p->mv >= 3300u && p->mv <= 4400u && (!usb_cable() || p->vbus) ? V_PASS : V_FAIL, b);
}

static void t_bt(void)
{
    int was_off = bt_state() == BT_OFF || bt_state() == BT_NO_MODULE;
    uint32_t t0 = time_ms();
    char b[30];

    st_screen("Self-test 6/6: Bluetooth", "Waking the module (up to 7 s)...", NULL);
    if (was_off)
        bt_power(1);
    while (bt_state() == BT_STARTING && time_ms() - t0 < 7000u)
        ap_pump(), cpu_wait_tick();
    if (bt_version() >= 0) {
        snprintf(b, sizeof b, "module answers (CV1%d)", bt_version());
        set(T_BT, V_PASS, b);
    } else {
        set(T_BT, V_FAIL, "module silent");
    }
    if (was_off)
        bt_power(0);
}

static void summary(void)
{
    int fails = 0;
    char b[48];

    display_fill_rect(0, 0, DISPLAY_W - 1, DISPLAY_H - 1, 0x0000);
    for (int t = 0; t < T_N; t++) {
        fails += R[t].v == V_FAIL;
        snprintf(b, sizeof b, "%-4s %-9s %s", R[t].v == V_PASS ? "ok" : R[t].v == V_FAIL ? "FAIL" : "-", k_name[t], R[t].d);
        st_line(t + 1, b, R[t].v == V_FAIL ? 0xF800 : R[t].v == V_PASS ? 0x07E0 : 0xC618);
    }
    snprintf(b, sizeof b, fails ? "Self-test: %d failed (any key)" : "Self-test: all good (any key)", fails);
    st_line(0, b, 0xFFFF);
    display_flush_rows(0, DISPLAY_H - 1);
    log_printf("selftest: done, %d failed", fails);
    log_flush("selftest");
    st_key(0xFFFFFFFFu);
}

void selftest_run(void)
{
    memset(R, 0, sizeof R);
    player_stop();
    power_screen(1);
    log_printf("selftest: start");
    t_keys();
    t_audio();
    t_disk(T_INT, "/Internal/GILB");
    if (sdcard_mounted())
        t_disk(T_SD, "/SD/GILB");
    else
        set(T_SD, V_SKIP, "no card");
    t_clock();
    t_battery();
    t_bt();
    set(T_USB, V_INFO, usb_cable() ? "cable in" : "no cable");
    summary();
    input_mock_clear();
}
