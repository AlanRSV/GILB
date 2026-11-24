/*
 * usb_probe_rknanoc.c -- usb_probe.h: the PHY's VBUS comparators swept per candidate supply.
 *
 * GRF (0x50010000, write-masked: the upper half says which low bits a write changes):
 *   USBPHY_CON6 +0x98: [6:3] force A-sess/B-sess/sess-end/VBUS-valid, [9:7] A-session
 *                      reference, [12:10] B-session reference, [15:13] session-end reference
 *   USBPHY_CON7 +0x9C: [2:0] VBUS-valid reference, [15] VBUS detection powered down
 *   UOC_CON1    +0xB4: [9] IDDIG from software, [10] IDDIG (0 = A-device: host)
 *   UOC_CON2    +0xB8: [1]/[2] D+/D- 15k pull-downs, [4] UTMI_DRVVBUS, [5] UTMI_CHRGBUS
 *   SOC_USB_STATUS +0xE8: [0] VBUS valid, [1] B-session valid, [2] A-session valid, [3] session end
 * (SDK driver/GRF/hw_grf.h.) No pin state here is one stock firmware does not use itself.
 */
#include <stdint.h>

#include "clock.h"
#include "panel_io.h"
#include "rknanoc.h"
#include "usb_msc.h"
#include "usb_probe.h"
#include "services/ovl.h"              /* OVL_TEXT: functions in overlays */

#define OTG(off)    (*(volatile uint32_t *)(0x60080000u + (off)))
#define GPIO_DR(p)  (*(volatile uint32_t *)(k_gpio[p]))
#define GPIO_DDR(p) (*(volatile uint32_t *)(k_gpio[p] + 4u))

enum { GUSBCFG = 0x0C, GRSTCTL = 0x10, DCTL = 0x804, HCFG = 0x400, HPRT = 0x440 };
enum { CON6 = 0x98 / 4, CON7 = 0x9C / 4, UOC1 = 0xB4 / 4, UOC2 = 0xB8 / 4, STATUS = 0xE8 / 4 };

static const uint32_t k_gpio[3] = { 0x40160000u, 0x40170000u, 0x50030000u };

uint32_t usb_probe_phy[15];

/* One comparator's trip pattern over its eight reference codes. */
static uint8_t sweep(unsigned reg, unsigned shift, unsigned bit)
{
    uint8_t m = 0;

    for (unsigned code = 0; code < 8u; code++) {
        RK_GRF[reg] = (7u << (shift + 16u)) | code << shift;
        rk_delay_us(200);
        if (RK_GRF[STATUS] & (1u << bit))
            m |= (uint8_t)(1u << code);
    }
    return m;
}

static void gpio_set(unsigned port, unsigned pin, int level)
{
    if (port == 0) {
        panel_io_gpio0_pin(pin, level);              /* (GPIO0 goes through the panel's shadow) */
    } else if (level) {
        GPIO_DR(port) |= 1u << pin;
    } else {
        GPIO_DR(port) &= ~(1u << pin);
    }
    GPIO_DDR(port) |= 1u << pin;
}

static int gpio_get(unsigned port, unsigned pin)
{
    return (int)((GPIO_DR(port) >> pin) & 1u);
}

/* The controller as host (the SDK's HostConnect(): IDDIG from software = 0, the 15k
 * pull-downs on D+/D-), then port power (the core's own drvvbus). */
static void host_on(void)
{
    RK_GRF[UOC2] = 0x00060006u;
    RK_GRF[UOC1] = 0x06000200u;
    OTG(GUSBCFG) |= 1u << 29;                        /* ForceHstMode */
    delay_ms(25);
    OTG(HCFG) &= ~3u;
    OTG(HPRT) = (OTG(HPRT) & ~0x2Eu) | 1u << 12;     /* PrtPwr (W1C/W1-disable bits left alone) */
}

static void host_off(void)
{
    OTG(HPRT) &= ~(0x2Eu | 1u << 12);
    OTG(GUSBCFG) &= ~(1u << 29);
    RK_GRF[UOC1] = 0x06000000u;                      /* IDDIG from the PHY again */
    RK_GRF[UOC2] = 0x00060000u;
}

/* The sequence a piece at a time (usb_probe.h): each step's pin state, 50 ms for VBUS to settle,
 * the comparator sweeps (~7 ms, at once), on a host step up to 200 ms for a device's D+, the pin
 * state back. The waits are the caller's: usb_probe_run() sleeps them, the boot's wake (startup/
 * ap_ui.c) runs its main loop meanwhile, so the screen and keys never stop for the ~0.9 s. */
static struct {
    usb_probe_step_t *out, one;
    void (*progress)(int step, const char *name);
    uint32_t dr1, dr2, ddr1, ddr2;
    uint8_t g21, g20, g014, k, phase, polls;
} W;

static const char *const k_name[USB_PROBE_STEPS][2] = {   /* [step][the pin's level as found] */
    { "baseline", "baseline" }, { "GPIO2.1 1", "GPIO2.1 0" }, { "GPIO2.0 1", "GPIO2.0 0" },
    { "GPIO0.14 1", "GPIO0.14 0" }, { "DRVVBUS", "DRVVBUS" }, { "host+port power", "host+port power" },
    { "all", "all" }, { "CHRGBUS (ref)", "CHRGBUS (ref)" },
};

/* Step k's pin state (on) or the state before it (off). */
static void act(unsigned k, int on)
{
    switch (k) {
    case 1: gpio_set(2, 1, on ? !W.g21 : W.g21); break;
    case 2: gpio_set(2, 0, on ? !W.g20 : W.g20); break;
    case 3: gpio_set(0, 14, on ? !W.g014 : W.g014); break;
    case 4: RK_GRF[UOC2] = on ? 0x00100010u : 0x00100000u; break;
    case 5: if (on) host_on(); break;
    case 6:
        if (on) {
            gpio_set(2, 1, 1);
            gpio_set(2, 0, 1);
            RK_GRF[UOC2] = 0x00100010u;
        } else {
            RK_GRF[UOC2] = 0x00100000u;
            host_off();
            gpio_set(2, 1, W.g21);
            gpio_set(2, 0, W.g20);
        }
        break;
    case 7: RK_GRF[UOC2] = on ? 0x00200020u : 0x00200000u; break;
    default: break;
    }
}

int usb_probe_start(usb_probe_step_t *out, void (*progress)(int step, const char *name))
{
    if (usb_cable())
        return -1;
    W.out = out;
    W.progress = progress;
    W.dr1 = GPIO_DR(1);
    W.dr2 = GPIO_DR(2);
    W.ddr1 = GPIO_DDR(1);
    W.ddr2 = GPIO_DDR(2);
    W.g014 = (uint8_t)gpio_get(0, 14);
    usb_dwc2_phy_on();                               /* (the comparators need the PHY up) */
    OTG(DCTL) |= 2u;                                 /* no D+ pull-up of our own */
    for (unsigned i = 0; i < 15u; i++)
        usb_probe_phy[i] = RK_GRF[(0x80u + 4u * i) / 4u] & 0xFFFFu;
    W.g21 = (uint8_t)gpio_get(2, 1);
    W.g20 = (uint8_t)gpio_get(2, 0);
    W.k = W.phase = 0;
    return 0;
}

static void finish(void)
{
    usb_dwc2_phy_off();
    GPIO_DR(1) = W.dr1;                              /* (the PHY bring-up drives GPIO1.4, 2.0, 2.2) */
    GPIO_DDR(1) = W.ddr1;
    GPIO_DR(2) = W.dr2;
    GPIO_DDR(2) = W.ddr2;
}

int usb_probe_step(uint32_t *wait_ms)
{
    unsigned k = W.k;
    usb_probe_step_t *s = W.out ? &W.out[k] : &W.one;
    const char *name = k_name[k][k == 1 ? W.g21 : k == 2 ? W.g20 : k == 3 ? W.g014 : 0];

    *wait_ms = 0;
    if (W.phase == 0) {                              /* the step's pin state, then VBUS settles */
        if (W.progress)
            W.progress((int)k, name);
        act(k, 1);
        W.phase = 1;
        *wait_ms = 50;
        return 0;
    }
    if (W.phase == 1) {
        uint32_t con6 = RK_GRF[CON6] & 0xFFFFu, con7 = RK_GRF[CON7] & 0xFFFFu;

        s->name = name;
        s->vbus = sweep(CON7, 0, 0);
        s->bsess = sweep(CON6, 10, 1);
        s->asess = sweep(CON6, 7, 2);
        s->sessend = sweep(CON6, 13, 3);
        RK_GRF[CON6] = 0xFFFF0000u | con6;           /* the references back */
        RK_GRF[CON7] = 0xFFFF0000u | con7;
        for (unsigned ch = 0; ch < 4u; ch++)
            s->adc[ch] = (int16_t)adc_value(ch);
        s->host = -1;
        s->linestate = 0;
        W.polls = 0;
        if (k == 5 || k == 6) {                      /* a host step: a device's D+, up to 200 ms */
            W.phase = 2;
            return 0;
        }
    } else {
        uint32_t p = OTG(HPRT);

        if (!(p & 1u) && ++W.polls < 20u) {
            *wait_ms = 10;
            return 0;
        }
        s->host = (int8_t)(p & 1u);
        s->linestate = (uint8_t)((p >> 10) & 3u);
    }
    act(k, 0);
    W.phase = 0;
    if (++W.k < USB_PROBE_STEPS)
        return 0;
    finish();
    return 1;
}

void usb_probe_abort(void)
{
    host_off();
    RK_GRF[UOC2] = 0x00300000u;                      /* DRVVBUS and CHRGBUS off */
    gpio_set(0, 14, W.g014);
    finish();
}

OVL_TEXT(dev) int usb_probe_run(usb_probe_step_t *out, void (*progress)(int step, const char *name))
{
    uint32_t w;

    if (usb_probe_start(out, progress))
        return -1;
    while (!usb_probe_step(&w))
        if (w)
            delay_ms(w);
    return USB_PROBE_STEPS;
}
