/* hello.c -- GILB's first program: a test of everything a program gets (sdk/include/gilb_app.h).
 *
 * Draws a screen of its own in bands, a square that bounces, the last key and the frame rate;
 * counts its runs in a file (HELLO.CNT next to it). BACK (held) or POWER (held 2 s) leaves. */
#include <stdint.h>
#include "gilb_app.h"

#define W 320
#define H 170
#define BAND 16

static uint16_t s_band[2][W * BAND];     /* two: one is drawn while the other goes to the panel */
static const gilb_api_t *A;

static uint16_t rgb(int r, int g, int b)
{
    return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

/* Text in band buffer b (its first row is screen row y0), 8x8 cells at x, y, scaled by s. */
static void text(uint16_t *b, int y0, int x, int y, const char *str, uint16_t fg, int s)
{
    for (; *str; str++, x += 8 * s) {
        const uint8_t *g = A->glyph(*str);

        for (int r = 0; r < 8 * s; r++) {
            int sy = y + r - y0;

            if (sy < 0 || sy >= BAND)
                continue;
            for (int c = 0; c < 8 * s; c++)
                if (g[r / s] & (0x80 >> (c / s)) && x + c >= 0 && x + c < W)
                    b[sy * W + x + c] = fg;
        }
    }
}

static void num(char *o, unsigned v)
{
    char t[12];
    int n = 0;

    do
        t[n++] = (char)('0' + v % 10);
    while (v /= 10);
    while (n)
        *o++ = t[--n];
    *o = 0;
}

static void cat(char *o, const char *a)
{
    while (*o)
        o++;
    while ((*o++ = *a++))
        ;
}

static const char *key_name(int k)
{
    static const char *const n[] = { "-", "VOL+", "VOL-", "PLAY", "BACK", "POWER", "MENU", "VOL+ VOL-" };

    return k >= 0 && k <= 7 ? n[k] : "?";
}

static const char *ges_name(int g)
{
    static const char *const n[] = { "", "down", "tap", "hold", "repeat", "release", "end", "chord" };

    return g >= 0 && g <= 7 ? n[g] : "?";
}

/* The run count in HELLO.CNT: read, add one, write back. */
static unsigned runs(void)
{
    static const char path[] = GILB_APP_DIR "/HELLO.CNT";
    unsigned n = 0;
    void *f = A->open(path, GF_READ);

    if (f) {
        if (A->read(f, &n, sizeof n) != (long)sizeof n)
            n = 0;
        A->close(f);
    }
    n++;
    if ((f = A->open(path, GF_WRITE)) != 0) {
        A->write(f, &n, sizeof n);
        A->close(f);
    }
    return n;
}

int app_main(const gilb_api_t *api)
{
    char l1[48] = "Run ", l2[48] = "Last key: -", l3[24] = "";
    int sx = 40, sy = 60, dx = 3, dy = 2, b = 0, held_back = 0;
    uint32_t t0, frames = 0;
    unsigned n;

    A = api;
    api->log("hello: start");
    n = runs();
    num(l1 + 4, n);
    cat(l1, n == 1 ? " (first time)" : "");
    t0 = api->ms();

    for (;;) {
        gilb_key_t k;

        if (api->poll())
            break;
        while (api->key(&k)) {
            if (k.key == GK_BACK && k.gesture == GE_HOLD)
                held_back = 1;
            l2[0] = 0;
            cat(l2, "Last key: ");
            cat(l2, key_name(k.key));
            cat(l2, " ");
            cat(l2, ges_name(k.gesture));
            if (k.gesture == GE_TAP && k.taps > 1) {
                char t[4] = { ' ', 'x', (char)('0' + k.taps), 0 };
                cat(l2, t);
            }
        }
        if (held_back)
            break;

        sx += dx;
        sy += dy;
        if (sx < 0 || sx > W - 24) { dx = -dx; sx += 2 * dx; }
        if (sy < 40 || sy > H - 24) { dy = -dy; sy += 2 * dy; }

        for (int y0 = 0; y0 < H; y0 += BAND, b ^= 1) {
            uint16_t *p = s_band[b];
            int rows = H - y0 < BAND ? H - y0 : BAND;

            for (int r = 0; r < rows; r++)
                for (int x = 0; x < W; x++) {
                    int y = y0 + r;
                    uint16_t c = rgb(x * 255 / W / 3, y * 255 / H / 4, 40 + y * 80 / H);

                    if (x >= sx && x < sx + 24 && y >= sy && y < sy + 24)
                        c = rgb(255, 120 + (frames & 63), 40);
                    p[r * W + x] = c;
                }
            text(p, y0, 8, 6, "Hello from a GILB program", 0xFFFF, 1);
            text(p, y0, 8, 22, l1, rgb(200, 220, 255), 1);
            text(p, y0, 8, H - 30, l2, 0xFFFF, 1);
            text(p, y0, 8, H - 14, "Hold BACK to leave", rgb(170, 170, 170), 1);
            text(p, y0, W - 8 - 8 * 6, 22, l3, rgb(140, 255, 140), 1);
            api->blit(0, y0, W, rows, p, W);
        }
        frames++;
        if (api->ms() - t0 >= 1000u) {
            l3[0] = 0;
            num(l3, frames);
            cat(l3, " fps");
            frames = 0;
            t0 = api->ms();
        }
    }
    api->sync();
    api->log("hello: bye");
    return 0;
}
