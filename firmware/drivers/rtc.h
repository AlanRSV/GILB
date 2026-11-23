/*
 * rtc.h -- the clock chip: a PCF8563-type real-time clock on I2C1 (7-bit address 0x51), kept running by
 * the battery while the player is off. Stock reads it the same way (HIFIEC40: I2C channel 5 = I2C1 on
 * GPIO2.12/2.13, registers 2..8 in BCD, 2026-01-01 12:00 written when it has no valid time).
 *
 * drivers/rtc_pcf8563.c, in the npx overlay (firmware_ap.ld); the simulator's is the PC's clock
 * (sim/sim_hw.c). Local time, years 2000..2099.
 */
#ifndef RECHORD_RTC_H
#define RECHORD_RTC_H

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t mon, day, hour, min, sec;
} rtc_time_t;

/* 0; -1 the chip did not answer; -2 it has no valid time (never set, or its supply failed). */
int rtc_read(rtc_time_t *t);
/* 0, or -1 (no answer). Starts the oscillator and clears the chip's "time lost" flag. */
int rtc_write(const rtc_time_t *t);

#endif
