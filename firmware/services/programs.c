/*
 * programs.c -- Settings > System > Programs: list the .APP files in GILB/APPS, load one, run it, come back.
 *
 * The whole file is the prg overlay (firmware_ap.ld), in the library job's phase window (OVL_LPH),
 * so none of it is resident. A program (sdk/include/gilb_app.h, docs/PROGRAMS.md) is linked at
 * GILB_APP_BASE and gets GILB_APP_BASE..GILB_APP_END: the audio input ring (the music stops first),
 * the modes' window, the file system's window and the library's window (ovl_take: their overlays
 * leave, and load again on demand afterwards; the library's here, at the end). This window, the
 * API table in it and all of resident memory stay put, so when the program's entry returns GILB
 * simply carries on.
 *
 * Checked before anything runs: the magic, the header's size, the API version it needs, where it
 * was linked, its length (the file's, and the room), its .bss end, its entry, then the CRC of every
 * byte read. A refusal says why on the screen and in the log.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../sdk/include/gilb_app.h"
#include "drivers/board.h"
#include "drivers/clock.h"
#include "drivers/display.h"
#include "drivers/font.h"
#include "drivers/panel_io.h"
#include "audio/audio.h"
#include "audio/audio_shared.h"
#include "player/player.h"
#include "services/fs.h"
#include "services/fw_update.h"
#include "services/input.h"
#include "services/log.h"
#include "services/ovl.h"
#include "services/power.h"

void ap_pump(void);                  /* startup/ap_ui.c: the music, storage queue, log, BT */

#define MAX_APPS 16
#define ROW_H    14

_Static_assert(sizeof(gilb_app_hdr_t) == 64, "the header is 64 bytes");

/* The list, on programs_run()'s stack (an overlay's .bss is resident, and resident RAM is full). */
typedef struct {
    char file[MAX_APPS][13];         /* 8.3 names */
    char title[MAX_APPS][32];
    int n;
} apps_t;
static apps_t *L;
#define s_file  (L->file)
#define s_title (L->title)
#define s_n     (L->n)
static uint32_t s_power_t0;          /* POWER held since (poll), 0 = up */

/* ---- the API a program gets ------------------------------------------------------------------ */

static void a_blit(int x, int y, int w, int h, const uint16_t *px, int pitch)
{
    if (!px || w <= 0 || h <= 0)
        return;
    if (x < 0) { px -= x; w += x; x = 0; }
    if (y < 0) { px -= (long)y * pitch; h += y; y = 0; }
    if (x + w > DISPLAY_W)
        w = DISPLAY_W - x;
    if (y + h > DISPLAY_H)
        h = DISPLAY_H - y;
    if (w <= 0 || h <= 0)
        return;
    for (int r = 0; r < h; r += DISPLAY_BAND_ROWS) {   /* (the display's own band size, proven) */
        int n = h - r < DISPLAY_BAND_ROWS ? h - r : DISPLAY_BAND_ROWS;
        panel_io_blit(x, y + r + PANEL_ROW_OFFSET, x + w - 1, y + r + n - 1 + PANEL_ROW_OFFSET,
                      px + (long)r * pitch, pitch);
    }
}

static void a_backlight(int pct)
{
    backlight_set(pct <= 0 ? 0 : pct >= 100 ? 100 : 100 - pct);
}

static const uint8_t *a_glyph(int ch)
{
    return font_glyph((uint8_t)ch);
}

static int a_key(gilb_key_t *ev)
{
    input_event_t e;

    if (input_next(&e) <= 0)
        return 0;
    ev->key = e.key;
    ev->gesture = e.gesture;
    ev->taps = e.taps;
    ev->steps = e.steps;
    ev->t_ms = e.t_ms;
    return 1;
}

static int a_poll(void)
{
    uint32_t now = time_ms();

    input_scan(keys_adc(), keys_power());
    ap_pump();
    if (!keys_power())
        s_power_t0 = 0;
    else if (!s_power_t0)
        s_power_t0 = now | 1u;
    return s_power_t0 && now - s_power_t0 >= 2000u;
}

static uint32_t a_ms(void)
{
    return time_ms();
}

static void a_sleep(uint32_t ms)
{
    uint32_t t0 = time_ms();

    do {
        a_poll();
        cpu_wait_tick();
    } while (time_ms() - t0 < ms);
}

static void *a_open(const char *path, int mode)
{
    if (!path)
        return NULL;
    return mode == GF_WRITE ? fs_open_write(path) : mode == GF_APPEND ? fs_open_append(path) : fs_open_read(path);
}

static long a_read(void *f, void *buf, unsigned long n) { return f ? fs_read(f, buf, n) : -1; }
static long a_write(void *f, const void *buf, unsigned long n) { return f ? fs_write(f, buf, n) : -1; }
static long a_seek(void *f, long off) { return f ? fs_seek(f, off) : -1; }
static long a_size(void *f) { return f ? fs_file_size(f) : -1; }
static int a_close(void *f) { return f ? fs_close(f) : -1; }
static int a_exists(const char *path) { return path ? fs_exists(path) : 0; }
static void a_log(const char *s) { log_printf("app: %s", s ? s : ""); }

/* Core B's RAM around the shared block (audio/audio_shared.h: AS_B_RAM, AS_SHARED 4 KiB, AS_B_TOP). */
static int a_more_mem(gilb_mem_t *out, int max)
{
    static const gilb_mem_t k[2] = {
        { AS_B_RAM, AS_SHARED - AS_B_RAM }, { AS_SHARED + 0x1000u, AS_B_TOP - AS_SHARED - 0x1000u },
    };
    int n = 0;

    audio_b_free();
    log_printf("programs: core B held, its RAM lent");
    for (; out && n < max && n < 2; n++)
        out[n] = k[n];
    return n;
}

static const gilb_api_t k_api = {
    .version = GILB_API_VERSION, .api_size = sizeof(gilb_api_t),
    .board = BOARD == BOARD_512M, .screen_w = DISPLAY_W, .screen_h = DISPLAY_H,
    .blit = a_blit, .sync = panel_io_sync, .backlight = a_backlight, .glyph = a_glyph,
    .key = a_key, .poll = a_poll, .ms = a_ms, .sleep_ms = a_sleep,
    .open = a_open, .read = a_read, .write = a_write, .seek = a_seek, .size = a_size, .close = a_close,
    .exists = a_exists, .log = a_log, .dir = GILB_APP_DIR, .more_mem = a_more_mem,
};

/* ---- the screens -------------------------------------------------------------------------------- */

static void text(int row, const char *s, uint16_t fg, uint16_t bg)
{
    display_draw_text(8, 8 + row * ROW_H, s, fg, bg);
}

static void message(const char *l1, const char *l2)
{
    display_clear(0x0000);
    text(0, "Programs", 0xFFFF, 0x0000);
    text(2, l1, 0xFFFF, 0x0000);
    if (l2)
        text(3, l2, 0xC618, 0x0000);
    text(10, "Any key: back", 0x8410, 0x0000);
    display_flush_rows(0, DISPLAY_H - 1);
}

/* The next key's release (a tap or a hold's end), or KEY_NONE after ms. */
static int next_key(uint32_t ms, int *steps)
{
    uint32_t t0 = time_ms();
    input_event_t ev;

    while (time_ms() - t0 < ms) {
        input_scan(keys_adc(), keys_power());
        while (input_next(&ev) > 0)
            if (ev.gesture == IG_TAP || ev.gesture == IG_REPEAT || ev.gesture == IG_HOLD) {
                power_activity();
                *steps = ev.gesture == IG_REPEAT ? ev.steps : 1;
                return ev.key;
            }
        ap_pump();
        cpu_wait_tick();
    }
    return KEY_NONE;
}

static void wait_any(void)
{
    int s;

    next_key(60000u, &s);
}

static void drain(void)
{
    input_event_t ev;

    while (input_next(&ev) > 0)
        ;
}

/* ---- the list ------------------------------------------------------------------------------------ */

static int ends_app(const char *n)
{
    size_t l = strlen(n);

    return l > 4 && n[l - 4] == '.' && (n[l - 3] | 32) == 'a' && (n[l - 2] | 32) == 'p' && (n[l - 1] | 32) == 'p';
}

static int add(void *u, const char *name, const fs_ref_t *r)
{
    (void)u;
    if (!(r->flags & FS_REF_DIR) && ends_app(name) && strlen(name) < sizeof s_file[0] && s_n < MAX_APPS)
        strcpy(s_file[s_n++], name);
    return 0;
}

/* The title in each file's header, else its name without .APP. */
static void titles(void)
{
    char path[64];
    gilb_app_hdr_t h;

    for (int i = 0; i < s_n; i++) {
        fs_file_t *f;

        snprintf(s_title[i], sizeof s_title[i], "%.*s", (int)strlen(s_file[i]) - 4, s_file[i]);
        snprintf(path, sizeof path, GILB_APP_DIR "/%.12s", s_file[i]);
        if ((f = fs_open_read(path)) == NULL)
            continue;
        if (fs_read(f, &h, sizeof h) == (long)sizeof h && h.magic == GILB_APP_MAGIC && h.name[0]) {
            h.name[sizeof h.name - 1] = 0;
            snprintf(s_title[i], sizeof s_title[i], "%s", h.name);
        }
        fs_close(f);
    }
}

static void draw_list(int sel, int top)
{
    display_clear(0x0000);
    text(0, "Programs", 0xFFFF, 0x0000);
    for (int i = 0; i < 8 && top + i < s_n; i++) {
        int on = top + i == sel;
        char b[40];

        if (on)
            display_fill_rect(0, 4 + (i + 2) * ROW_H, DISPLAY_W - 1, 4 + (i + 3) * ROW_H - 1, 0xFFFF);
        snprintf(b, sizeof b, "%s", s_title[top + i]);
        text(i + 2, b, on ? 0x0000 : 0xFFFF, on ? 0xFFFF : 0x0000);
    }
    text(11, "PLAY start   BACK leave", 0x8410, 0x0000);
    display_flush_rows(0, DISPLAY_H - 1);
}

/* ---- loading and running -------------------------------------------------------------------------- */

static uint32_t app_crc(const uint8_t *p, uint32_t n)
{
    static const uint8_t zero[4];
    uint32_t c = fwu_crc(0, p, offsetof(gilb_app_hdr_t, crc));

    c = fwu_crc(c, zero, 4);
    return fwu_crc(c, p + offsetof(gilb_app_hdr_t, crc) + 4u, n - offsetof(gilb_app_hdr_t, crc) - 4u);
}

/* The reason a header is refused, or NULL. */
static const char *check(const gilb_app_hdr_t *h, long size)
{
    if (h->magic != GILB_APP_MAGIC || h->hdr_size != sizeof *h)
        return "Not a GILB program";
    if (h->api_min > GILB_API_VERSION)
        return "Needs a newer GILB";
    if (h->base != GILB_APP_BASE)
        return "Built for another address";
    if (size < (long)sizeof *h || h->load_end != GILB_APP_BASE + (uint32_t)size)
        return "Damaged (length)";
    if (h->load_end > GILB_APP_END || h->mem_end < h->load_end || h->mem_end > GILB_APP_END)
        return "Too big for the player";
    if ((h->entry & 1u) == 0 || h->entry < GILB_APP_BASE + sizeof *h || h->entry >= h->load_end)
        return "Damaged (entry)";
    return NULL;
}

/* Run program i; a message on the screen if it can't. */
static void run(int i)
{
    char path[64], b[48];
    gilb_app_hdr_t h;
    const char *why = NULL;
    uint8_t *const base = (uint8_t *)GILB_APP_BASE;
    fs_file_t *f;
    long size = -1;
    int rc;

    snprintf(path, sizeof path, GILB_APP_DIR "/%.12s", s_file[i]);
    if ((f = fs_open_read(path)) == NULL) {
        message("Can't open the file", s_file[i]);
        wait_any();
        return;
    }
    size = fs_file_size(f);
    if (fs_read(f, &h, sizeof h) != (long)sizeof h)
        why = "Can't read the file";
    else
        why = check(&h, size);
    if (why) {
        fs_close(f);
        log_printf("programs: %s refused: %s", s_file[i], why);
        message(why, s_file[i]);
        wait_any();
        return;
    }

    message("Starting...", s_title[i]);
    player_stop();                   /* (the input ring is the program's now) */
    ovl_take(1);
    if (fs_seek(f, 0) != 0 || fs_read(f, base, (unsigned long)size) != size)
        why = "Can't read the file";
    else if (app_crc(base, (uint32_t)size) != h.crc)
        why = "Damaged (CRC)";
    fs_close(f);
    if (!why) {
        int (*entry)(const gilb_api_t *) = (int (*)(const gilb_api_t *))h.entry;

        memset(base + size, 0, h.mem_end - h.load_end);
        log_printf("programs: %s start (%ld B, %lu B of memory)", s_file[i], size,
                   (unsigned long)(h.mem_end - GILB_APP_BASE));
        drain();
        s_power_t0 = 0;
        rc = entry(&k_api);
        panel_io_sync();
        log_printf("programs: %s ended (%d)", s_file[i], rc);
    }
    ovl_take(0);
    (void)ovl_get(OVL_LIB);          /* the library's code back (its callers only use ovl_if) */
    power_brightness_changed();      /* the user's brightness again */
    drain();
    if (why) {
        log_printf("programs: %s refused: %s", s_file[i], why);
        message(why, s_file[i]);
        wait_any();
    }
}

void programs_run(void)
{
    apps_t list;
    int sel = 0, top = 0;

    L = &list;
    power_screen(1);
    s_n = 0;
    if (fs_scan(GILB_APP_DIR, add, NULL) < 0 || !s_n) {   /* (fs_list_dir_each is the viz overlay's) */
        message("No programs yet.", "Copy .APP files to GILB/APPS.");
        wait_any();
        L = NULL;
        return;
    }
    titles();
    for (;;) {
        int steps, k;

        draw_list(sel, top);
        k = next_key(0xFFFFFFFFu, &steps);
        if (k == KEY_UP)
            sel = sel - steps < 0 ? 0 : sel - steps;
        else if (k == KEY_DOWN)
            sel = sel + steps >= s_n ? s_n - 1 : sel + steps;
        else if (k == KEY_SELECT)
            run(sel);
        else if (k == KEY_BACK || k == KEY_POWER || k == KEY_MENU)
            break;
        if (sel < top)
            top = sel;
        else if (sel >= top + 8)
            top = sel - 7;
    }
    drain();
    L = NULL;
}
