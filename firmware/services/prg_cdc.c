/*
 * prg_cdc.c -- a USB serial port for programs (sdk/include/gilb_app.h API version 4, serial_*).
 *
 * drivers/usb_cdc_rknanoc.c (Settings > Developer > USB console) once more, under other names, in
 * the prg overlay: the Developer overlay's window is the program's memory while one runs, and the
 * prg overlay may call resident code only (firmware_ap.ld NOCROSSREFS). Its state goes into the
 * overlay too (firmware_ap.ld puts this file's .bss there): no resident RAM, and fresh at each
 * load. The PC sees a COM port (CDC-ACM, Windows' own driver), as with the Developer console.
 */
#define usb_cdc_init         prg_cdc_init
#define usb_cdc_poll         prg_cdc_poll
#define usb_cdc_run          prg_cdc_run
#define usb_cdc_puts         prg_cdc_puts
#define usb_cdc_getc         prg_cdc_getc
#define usb_cdc_available    prg_cdc_available
#define usb_cdc_request_exit prg_cdc_request_exit
#define usb_cdc_diag         prg_cdc_diag
#include <string.h>
#include "../drivers/usb_cdc_rknanoc.c"

/* A clean start: the overlay stays in its window from one program to the next, and so did the
 * last session's state ("configured" before the PC had seen the port). */
void prg_cdc_open(void)
{
    memset(&s, 0, sizeof s);
    memset(&s_diag, 0, sizeof s_diag);
    prg_cdc_init();
}

int prg_cdc_configured(void)
{
    return s.configured;
}

/* Up to 64 bytes a packet; -1 when the host is not there or the transfer failed. */
long prg_cdc_write(const void *buf, unsigned long n)
{
    const uint8_t *p = buf;
    unsigned long done = 0;

    if (!s.configured)
        return -1;
    while (done < n) {
        uint32_t k = n - done < 64u ? (uint32_t)(n - done) : 64u;

        if (cdc_ep_in(EP_DATA_IN, p + done, k) != 0)
            return done ? (long)done : -1;
        done += k;
    }
    return (long)done;
}
