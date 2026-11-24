/*
 * wallclock.c -- the time of day (wallclock.h).
 */
#include <stdio.h>

#include "wallclock.h"
#include "config.h"
#include "drivers/clock.h"
#include "services/ovl.h"              /* OVL_TEXT: functions in overlays */

static uint32_t s_secs, s_at;
static uint8_t s_ok;

OVL_TEXT(npx) void wallclock_set(uint32_t secs)
{
    s_secs = secs;
    s_at = time_ms();
    s_ok = 1;
}

int wallclock_now(uint32_t *secs)
{
    if (!s_ok)
        return -1;
    *secs = s_secs + (time_ms() - s_at) / 1000u;
    return 0;
}

void wallclock_text(char *out, unsigned long n)
{
    uint32_t s;
    unsigned h, m;

    out[0] = 0;
    if (g_cfg.clock >= CFG_CLOCK_OFF || wallclock_now(&s))
        return;
    h = (unsigned)(s / 3600u % 24u);
    m = (unsigned)(s / 60u % 60u);
    if (g_cfg.clock == CFG_CLOCK_24)
        snprintf(out, n, "%02u:%02u", h, m);
    else
        snprintf(out, n, "%u:%02u %s", (h + 11u) % 12u + 1u, m, h < 12u ? "AM" : "PM");
}
