/*
 * wallclock.h -- the time of day: the clock chip's time (drivers/rtc.h), read at boot and after a USB
 * disk session by the npx overlay (services/clock_set.c), kept by time_ms() in between. Seconds
 * since 2000-01-01 00:00, local time. Resident: the header shows it (ui/frame.c).
 */
#ifndef RECHORD_WALLCLOCK_H
#define RECHORD_WALLCLOCK_H

#include <stdint.h>

void wallclock_set(uint32_t secs);             /* the time now */
int wallclock_now(uint32_t *secs);             /* 0, or -1 while it is not known */
/* "3:42 PM" or "15:42" as g_cfg.clock says (CFG_CLOCK_*); "" when it is off or not known. */
void wallclock_text(char *out, unsigned long n);

#endif
