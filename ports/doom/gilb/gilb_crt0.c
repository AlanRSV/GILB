/*
 * gilb_crt0.c -- Doom's start file: sdk/crt0.c's header, with faster memory functions.
 *
 * The SDK's crt0.c copies a byte at a time, which is fine for small programs. Doom copies a lot
 * (the level data into its cache, WAD pages, colour maps), so this one moves words when it can.
 * The header is the SDK's, field for field (sdk/include/gilb_app.h).
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 */
#include <stddef.h>
#include <stdint.h>
#include "gilb_app.h"

extern char __load_end[], __mem_end[];   /* doom.ld */
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
    .crc = 0,                        /* (sdk/tools/gilb_app.py) */
    .name = APP_NAME,
};

/* (The compiler must not turn these loops back into calls to themselves.) */
#define NOLOOP __attribute__((optimize("no-tree-loop-distribute-patterns")))

NOLOOP void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    uint32_t w = (uint8_t)c * 0x01010101u;

    while (n && ((uintptr_t)p & 3u)) {
        *p++ = (uint8_t)c;
        n--;
    }
    for (; n >= 16; n -= 16, p += 16) {
        ((uint32_t *)p)[0] = w;
        ((uint32_t *)p)[1] = w;
        ((uint32_t *)p)[2] = w;
        ((uint32_t *)p)[3] = w;
    }
    for (; n >= 4; n -= 4, p += 4)
        *(uint32_t *)p = w;
    while (n--)
        *p++ = (uint8_t)c;
    return d;
}

NOLOOP void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;

    if ((((uintptr_t)p ^ (uintptr_t)q) & 3u) == 0) {   /* the same alignment: words */
        while (n && ((uintptr_t)p & 3u)) {
            *p++ = *q++;
            n--;
        }
        for (; n >= 16; n -= 16, p += 16, q += 16) {
            uint32_t a = ((const uint32_t *)q)[0], b = ((const uint32_t *)q)[1];
            uint32_t c = ((const uint32_t *)q)[2], e = ((const uint32_t *)q)[3];

            ((uint32_t *)p)[0] = a;
            ((uint32_t *)p)[1] = b;
            ((uint32_t *)p)[2] = c;
            ((uint32_t *)p)[3] = e;
        }
        for (; n >= 4; n -= 4, p += 4, q += 4)
            *(uint32_t *)p = *(const uint32_t *)q;
    }
    while (n--)
        *p++ = *q++;
    return d;
}

NOLOOP void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;

    if (p <= q || p >= q + n)
        return memcpy(d, s, n);
    while (n--)
        p[n] = q[n];
    return d;
}

NOLOOP int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;

    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}
