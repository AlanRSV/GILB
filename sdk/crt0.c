/* crt0.c -- every GILB program's start: the header GILB reads (sdk/include/gilb_app.h) and the
 * few C library functions the compiler may call on its own. Your program defines app_main(). */
#include <stddef.h>
#include <stdint.h>
#include "gilb_app.h"

#ifndef APP_NAME
#define APP_NAME ""
#endif
#ifndef APP_API_MIN
#define APP_API_MIN 1
#endif

extern char __load_end[], __mem_end[];   /* app.ld */
int app_main(const gilb_api_t *api);

__attribute__((section(".gilb_hdr"), used))
const gilb_app_hdr_t gilb_hdr = {
    .magic = GILB_APP_MAGIC,
    .hdr_size = sizeof(gilb_app_hdr_t),
    .api_min = APP_API_MIN,
    .base = GILB_APP_BASE,
    .load_end = (uint32_t)__load_end,
    .mem_end = (uint32_t)__mem_end,
    .entry = (uint32_t)app_main,
    .crc = 0,                        /* (tools/gilb_app.py) */
    .name = APP_NAME,
};

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;

    while (n--)
        *p++ = (uint8_t)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;

    while (n--)
        *p++ = *q++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;

    if (p < q)
        while (n--)
            *p++ = *q++;
    else
        while (n--)
            p[n] = q[n];
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;

    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}
