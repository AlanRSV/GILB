/*
 * clock_set.c -- the time of day from the clock chip, or from the PC (viz overlay, firmware_ap.ld:
 * the code budget). Runs at boot and after a USB disk session (startup/ap_ovl.c ap_npx_clock).
 *
 * Echo Library's "Set the player's clock" writes TIME_PATH, one line "YYYY-MM-DD HH:MM:SS" (the PC's
 * local time when it was written); found here, it sets the chip and is removed. Otherwise the
 * chip's own time is read (drivers/rtc.h).
 */
#include <stdint.h>

#include "wallclock.h"
#include "fs.h"
#include "log.h"
#include "drivers/rtc.h"
#include "ui/np_extra.h"

#define TIME_PATH "/Internal/GILB/TIME.TXT"

/* Seconds since 2000-01-01 00:00 (years 2000..2099: every fourth a leap year). */
static uint32_t secs_of(const rtc_time_t *t)
{
    static const uint16_t before[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    unsigned y = t->year - 2000u;
    uint32_t d = y * 365u + (y + 3u) / 4u + before[t->mon - 1u] + (t->mon > 2u && !(y % 4u)) + t->day - 1u;

    return ((d * 24u + t->hour) * 60u + t->min) * 60u + t->sec;
}

static int parse(const char *s, rtc_time_t *t)
{
    unsigned v[6];
    int k = 0;

    while (*s && k < 6) {
        if (*s >= '0' && *s <= '9') {
            unsigned x = 0;

            while (*s >= '0' && *s <= '9' && x < 10000u)
                x = x * 10u + (unsigned)(*s++ - '0');
            v[k++] = x;
        } else {
            s++;
        }
    }
    if (k < 6 || v[0] < 2000u || v[0] > 2099u || !v[1] || v[1] > 12u || !v[2] || v[2] > 31u || v[3] > 23u ||
        v[4] > 59u || v[5] > 59u)
        return -1;
    t->year = (uint16_t)v[0];
    t->mon = (uint8_t)v[1];
    t->day = (uint8_t)v[2];
    t->hour = (uint8_t)v[3];
    t->min = (uint8_t)v[4];
    t->sec = (uint8_t)v[5];
    return 0;
}

void npx_clock(void)
{
    rtc_time_t t;
    char buf[48];
    fs_file_t *f = fs_open_read(TIME_PATH);
    int r;

    if (f) {
        long n = fs_read(f, buf, sizeof buf - 1u);

        fs_close(f);
        buf[n > 0 ? n : 0] = 0;
        r = fs_remove(TIME_PATH);
        if (!parse(buf, &t)) {
            int w = rtc_write(&t);

            wallclock_set(secs_of(&t));
            log_printf("clock: set from TIME.TXT to %04u-%02u-%02u %02u:%02u:%02u (chip %s, file %s)", t.year,
                       t.mon, t.day, t.hour, t.min, t.sec, w ? "did not answer" : "set",
                       r ? "not removed" : "removed");
            return;
        }
        log_printf("clock: TIME.TXT not understood (%ld bytes)", n);
    }
    r = rtc_read(&t);
    if (!r) {
        wallclock_set(secs_of(&t));
        log_printf("clock: %04u-%02u-%02u %02u:%02u:%02u", t.year, t.mon, t.day, t.hour, t.min, t.sec);
    } else {
        log_printf("clock: %s", r == -1 ? "the chip did not answer" : "the chip has no time: set it from Echo Library");
    }
}
