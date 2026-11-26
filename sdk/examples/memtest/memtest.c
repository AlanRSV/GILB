/* memtest.c -- can a program use the audio core's RAM (api->more_mem, API version 2)?
 *
 * 1. Asks for the memory and lists the ranges.
 * 2. Writes a pattern over all of it and reads it back.
 * 3. Times the same loop reading the same data, run from GILB_APP_BASE (core A's own RAM) and then
 *    copied to core B's RAM and run there. The step before each run is logged first, so if running
 *    code there faults, the log (ECHOLOG.TXT) says where it stopped.
 *
 * Results on the screen and in the log ("app: memtest ..."). Hold BACK to leave. */
#include <stdint.h>
#include "gilb_app.h"

#define W 320
#define LINES 11

static const gilb_api_t *A;
static char s_line[LINES][41];
static int s_n;
static uint16_t s_band[2][W * 16];    /* two: one is drawn while the other goes to the panel */
static uint32_t s_data[1024];

extern char __movable_start[], __movable_end[];   /* app.ld */

/* The timed loop: copied as it is (Thumb, no absolute addresses), so it must stay self-contained. */
__attribute__((section(".movable"), noinline))
uint32_t work(uint32_t n, const uint32_t *p)
{
    uint32_t s = 0;

    for (uint32_t i = 0; i < n; i++)
        s += p[i & 1023u] ^ (s << 3) ^ i;
    return s;
}

static void put(char **o, const char *a)
{
    while (*a)
        *(*o)++ = *a++;
}

static void putu(char **o, uint32_t v, int base)
{
    char t[12];
    int n = 0;

    do
        t[n++] = "0123456789abcdef"[v % base];
    while (v /= base);
    while (n)
        *(*o)++ = t[--n];
}

/* A line on the screen and in the log, then a redraw. */
static void say(const char *a, uint32_t v1, const char *b, uint32_t v2, const char *c, int hex)
{
    char *o = s_line[s_n < LINES ? s_n : LINES - 1], *o0 = o;
    char logl[56] = "memtest: ", *l = logl + 9;

    put(&o, a);
    if (b) {
        putu(&o, v1, hex ? 16 : 10);
        put(&o, b);
        if (c) {
            putu(&o, v2, hex ? 16 : 10);
            put(&o, c);
        }
    }
    *o = 0;
    if (s_n < LINES)
        s_n++;
    for (const char *p = o0; *p && l < logl + sizeof logl - 1; )
        *l++ = *p++;
    *l = 0;
    A->log(logl);

    for (int y0 = 0, b = 0; y0 < 170; y0 += 16, b ^= 1) {
        int rows = 170 - y0 < 16 ? 170 - y0 : 16;
        uint16_t *band = s_band[b];

        for (int i = 0; i < W * 16; i++)
            band[i] = 0;
        for (int ln = 0; ln < s_n; ln++)
            for (int r = 0; r < 8; r++) {
                int y = 4 + ln * 14 + r - y0;

                if (y < 0 || y >= rows)
                    continue;
                for (int ch = 0; s_line[ln][ch]; ch++) {
                    const uint8_t *g = A->glyph(s_line[ln][ch]);

                    for (int c = 0; c < 8; c++)
                        if (g[r] & (0x80 >> c))
                            band[y * W + 6 + ch * 8 + c] = ln == 0 ? 0xFFE0 : 0xFFFF;
                }
            }
        A->blit(0, y0, W, rows, band, W);
    }
    A->sync();
}

int app_main(const gilb_api_t *api)
{
    gilb_mem_t m[4];
    int n;
    uint32_t bad = 0, total = 0;

    A = api;
    say("Memory test (BACK held: leave)", 0, 0, 0, 0, 0);
    if (api->version < 2 || api->api_size < sizeof(gilb_api_t)) {
        say("This GILB has no more_mem: update it", 0, 0, 0, 0, 0);
        goto wait;
    }
    n = api->more_mem(m, 4);
    for (int i = 0; i < n; i++)
        say("RAM block: ", m[i].size / 1024u, " KB", 0, 0, 0);

    /* 2: write, then read back (a pattern that differs per address) */
    for (int i = 0; i < n; i++) {
        volatile uint32_t *p = (volatile uint32_t *)m[i].base;
        uint32_t words = m[i].size / 4u;

        for (uint32_t w = 0; w < words; w++)
            p[w] = (m[i].base + w * 4u) * 2654435761u;
        for (uint32_t w = 0; w < words; w++)
            bad += p[w] != (m[i].base + w * 4u) * 2654435761u;
        total += m[i].size;
        api->poll();
    }
    say("write/read ", total / 1024u, " KB, ", bad, " bad words", 0);

    /* 3: the same loop from core A's RAM, then from core B's */
    for (int i = 0; i < 1024; i++)
        s_data[i] = (uint32_t)i * 40503u;
    {
        const uint32_t N = 2000000u;
        uint32_t len = (uint32_t)(__movable_end - __movable_start);
        uint32_t off = (uint32_t)((char *)work - __movable_start) & ~1u;
        uint32_t t0, ta, tb, ra, rb;
        uint32_t (*fb)(uint32_t, const uint32_t *);

        t0 = api->ms();
        ra = work(N, s_data);
        ta = api->ms() - t0;
        say("code in A RAM: ", ta, " ms (", ra, ")", 0);

        if (n < 1 || len > m[0].size) {
            say("no room to copy the code", 0, 0, 0, 0, 0);
            goto wait;
        }
        for (uint32_t i = 0; i < len; i++)
            ((volatile char *)m[0].base)[i] = __movable_start[i];
        __asm__ volatile("dsb\n isb" ::: "memory");
        fb = (uint32_t (*)(uint32_t, const uint32_t *))(m[0].base + off + 1u);
        say("running code in B RAM...", 0, 0, 0, 0, 0);
        t0 = api->ms();
        rb = fb(N, s_data);
        tb = api->ms() - t0;
        say("code in B RAM: ", tb, " ms (", rb, ")", 0);
        say(rb == ra ? "same result: B RAM runs code" : "DIFFERENT result!", 0, 0, 0, 0, 0);

        /* data in B's RAM, code in A's */
        for (int i = 0; i < 1024; i++)
            ((volatile uint32_t *)m[n - 1].base)[i] = s_data[i];
        t0 = api->ms();
        rb = work(N, (const uint32_t *)m[n - 1].base);
        tb = api->ms() - t0;
        say("data in B RAM: ", tb, " ms (", rb == ra, " = same)", 0);
    }

wait:
    for (;;) {
        gilb_key_t k;

        if (api->poll())
            break;
        if (api->key(&k) && k.key == GK_BACK && k.gesture == GE_HOLD)
            break;
        api->sleep_ms(20);
    }
    return 0;
}
