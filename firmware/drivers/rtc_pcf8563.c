/*
 * rtc_pcf8563.c -- the clock chip on I2C1 (drivers/rtc.h). In the npx overlay: it runs at boot and
 * after a USB disk session only.
 *
 * From stock (HIFIEC40, its I2C open for channel 5 and its time read/write): I2C1 at 0x40050000,
 * PCLK_I2C1 gate 0x84 and soft reset 27, GRF_IOMUX_CON (GRF+0xBC) bits 3:2 = 2 for I2C1's third pin
 * set, GPIO2.12 and GPIO2.13 to function 3; the chip at 0xA2 (write) on a 100 kHz bus. Registers:
 * 0 control 1 (0 = running), 2 seconds (bit 7: the time was lost), 3 minutes, 4 hours, 5 day,
 * 6 weekday, 7 month (bit 7: century), 8 year, all BCD. The controller is the same RK I2C block
 * as I2C0 (drivers/dac_cs43131.c, whose transfers these follow).
 */
#include <stdint.h>

#include "rtc.h"
#include "clock.h"

#define REG32(a)       (*(volatile uint32_t *)(uintptr_t)(a))
#define CRU_BASE       0x40180000u
#define CRU_GATE(g)    REG32(CRU_BASE + 0x80u + 4u * (unsigned)(g))
#define CRU_RST(r)     REG32(CRU_BASE + 0xC0u + 4u * (unsigned)(r))
#define GRF_BASE       0x50010000u
#define I2C1           0x40050000u
#define I_CON          REG32(I2C1 + 0x00u)
#define I_CLKDIV       REG32(I2C1 + 0x04u)
#define I_MRXADDR      REG32(I2C1 + 0x08u)
#define I_MRXRADDR     REG32(I2C1 + 0x0Cu)
#define I_MTXCNT       REG32(I2C1 + 0x10u)
#define I_MRXCNT       REG32(I2C1 + 0x14u)
#define I_IEN          REG32(I2C1 + 0x18u)
#define I_IPD          REG32(I2C1 + 0x1Cu)
#define I_TXFIFO(i)    REG32(I2C1 + 0x100u + 4u * (unsigned)(i))
#define I_RXFIFO(i)    REG32(I2C1 + 0x200u + 4u * (unsigned)(i))
#define CON_EN         (1u << 0)
#define CON_TRX        (1u << 1)          /* address, register, repeated start, receive */
#define CON_START      (1u << 3)
#define CON_STOP       (1u << 4)
#define CON_LASTACK    (1u << 5)
#define IPD_MTX        (1u << 2)
#define IPD_MRX        (1u << 3)
#define IPD_START      (1u << 4)
#define IPD_STOP       (1u << 5)
#define IPD_NAK        (1u << 6)
#define G_PCLK_I2C1    132
#define R_I2C1         27
#define CHIP           0xA2u

static int wait(uint32_t mask)
{
    for (uint32_t n = 400000; n; n--) {
        uint32_t p = I_IPD;

        if (p & IPD_NAK) {
            I_IPD = p;
            return -1;
        }
        if (p & mask) {
            I_IPD = p;
            return 0;
        }
    }
    return -1;
}

static void stop(void)
{
    I_IPD = 0x7Fu;
    I_CON = CON_EN | CON_STOP;
    (void)wait(IPD_STOP);
    I_IPD = 0x7Fu;
    I_CON = 0;
}

static int start(uint32_t mode)
{
    I_IPD = 0x7Fu;
    I_CON = CON_EN | CON_START | mode;
    if (wait(IPD_START)) {
        stop();
        return -1;
    }
    I_IPD = IPD_START;
    I_CON = CON_EN | mode;
    return 0;
}

/* The bus up: clock, reset, the pins, 100 kHz at the pclk of the moment. */
static void bus_up(void)
{
    uint32_t div = (clock_pclk_hz() / 1000u + 799u) / 800u - 2u;
    volatile uint32_t *mux = (volatile uint32_t *)(uintptr_t)(GRF_BASE + 2u * 0x20u + 1u * 4u);

    CRU_GATE(G_PCLK_I2C1 >> 4) = (CRU_GATE(G_PCLK_I2C1 >> 4) & ~(1u << (G_PCLK_I2C1 & 15)) & 0xFFFFu) | 0xFFFF0000u;
    CRU_RST(R_I2C1 >> 4) = 0x10001u << (R_I2C1 & 15);
    delay_us(10);
    CRU_RST(R_I2C1 >> 4) = 0x10000u << (R_I2C1 & 15);
    REG32(GRF_BASE + 0xBCu) = 0x000C0008u;                      /* I2C1: pin set 2 */
    *mux = (3u | 0x30000u) << ((12u & 7u) * 2u);                 /* GPIO2.12: function 3 */
    *mux = (3u | 0x30000u) << ((13u & 7u) * 2u);                 /* GPIO2.13: function 3 */
    I_CON = 0;
    I_IEN = 0;
    I_CLKDIV = (div >> 1) | (((div + 1u) >> 1) << 16);
}

static int write_regs(uint8_t reg, const uint8_t *v, unsigned n)
{
    uint32_t w = CHIP | (uint32_t)reg << 8;
    unsigned b = 2, k = 0;
    int r;

    bus_up();
    if (start(0))
        return -1;
    for (unsigned i = 0; i < n; i++) {
        w |= (uint32_t)v[i] << (8u * b);
        if (++b == 4) {
            I_TXFIFO(k++) = w;
            w = 0;
            b = 0;
        }
    }
    if (b)
        I_TXFIFO(k) = w;
    I_IPD = 0x7Fu;
    I_MTXCNT = n + 2u;
    r = wait(IPD_MTX);
    stop();
    return r;
}

static int read_regs(uint8_t reg, uint8_t *v, unsigned n)
{
    bus_up();
    I_MRXADDR = 0x01000000u | (CHIP | 1u);
    I_MRXRADDR = 0x01000000u | reg;
    if (start(CON_TRX))
        return -1;
    I_IPD = 0x7Fu;
    I_CON |= CON_LASTACK;
    I_MRXCNT = n;
    if (wait(IPD_MRX)) {
        stop();
        return -1;
    }
    for (unsigned k = 0; k < n; k += 4u) {
        uint32_t w = I_RXFIFO(k / 4u);

        for (unsigned j = 0; j < 4u && k + j < n; j++)
            v[k + j] = (uint8_t)(w >> (8u * j));
    }
    stop();
    return 0;
}

static unsigned bcd(uint8_t v, uint8_t mask) { v &= mask; return (v >> 4) * 10u + (v & 15u); }
static uint8_t tobcd(unsigned v) { return (uint8_t)((v / 10u) << 4 | v % 10u); }

int rtc_read(rtc_time_t *t)
{
    uint8_t r[7];

    if (read_regs(2, r, 7))
        return -1;
    t->sec = (uint8_t)bcd(r[0], 0x7F);
    t->min = (uint8_t)bcd(r[1], 0x7F);
    t->hour = (uint8_t)bcd(r[2], 0x3F);
    t->day = (uint8_t)bcd(r[3], 0x3F);
    t->mon = (uint8_t)bcd(r[5], 0x1F);
    t->year = (uint16_t)(2000u + bcd(r[6], 0xFF));
    if ((r[0] & 0x80u) || t->sec > 59u || t->min > 59u || t->hour > 23u || !t->day || t->day > 31u ||
        !t->mon || t->mon > 12u || t->year > 2099u)
        return -2;
    return 0;
}

int rtc_write(const rtc_time_t *t)
{
    static const uint8_t run = 0;
    uint8_t r[7];

    r[0] = tobcd(t->sec);                    /* (bit 7 clear: the time is good again) */
    r[1] = tobcd(t->min);
    r[2] = tobcd(t->hour);
    r[3] = tobcd(t->day);
    r[4] = 0;                                /* the weekday: unused */
    r[5] = tobcd(t->mon);
    r[6] = tobcd(t->year % 100u);
    if (write_regs(0, &run, 1))
        return -1;
    return write_regs(2, r, 7);
}
