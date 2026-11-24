/*
 * theme.c — theme registry + current-theme slot.
 *
 * The current theme lives in one writable slot and theme_get() hands out a
 * const pointer to it: consumers (UI, widgets) may cache the pointer for the
 * duration of one frame/redraw, and a theme switch is a plain struct copy —
 * no dynamic allocation anywhere (the DAP has no heap contract yet).
 *
 * The themes themselves (the built-in table, RECHORD/THEMES/ files, the
 * parser) are theme_store.c's, in an overlay on the device: this file keeps
 * the current theme and every theme's name (theme_rescan), and asks the store
 * through ap_theme_scan / ap_theme_load (startup/ap_ovl.c; the host test
 * links them to the store directly).
 */
#include <string.h>

#include "theme.h"
#include "theme_priv.h"

static theme_t g_current;
static int     g_have_current = 0;   /* lazy default to built-in 0 */
static char    s_names[THEME_MAX][THEME_NAME_MAX];
static int     s_n = -1;             /* themes listed; -1: not yet (or the store was unavailable) */

int theme_rescan(void)
{
    s_n = ap_theme_scan(s_names, THEME_MAX);
    return s_n < 0 ? 0 : s_n;
}

static int listed(void)
{
    return s_n < 0 ? theme_rescan() : s_n;
}

/* ASCII case-insensitive compare (theme names are ASCII by convention). */
static int name_eq(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = (unsigned char)*a++;
        int cb = (unsigned char)*b++;
        if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
        if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
        if (ca != cb)
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

const theme_t *theme_get(void)
{
    if (!g_have_current && theme_load_index(0) != 0) {
        g_current.text = g_current.sel_text = g_current.value = 0xFFFF;   /* (no store: readable) */
        g_have_current = 1;
    }
    return &g_current;
}

void theme_set(const theme_t *t)
{
    if (t == 0)
        return;
    g_current = *t;
    g_have_current = 1;
}

int theme_count(void)
{
    return listed();
}

const char *theme_name(int i)
{
    return i >= 0 && i < listed() ? s_names[i] : 0;
}

int theme_index(const char *name)
{
    int i, n = listed();

    if (name == 0)
        return -1;
    for (i = 0; i < n; i++)
        if (name_eq(s_names[i], name))
            return i;
    return -2;
}

int theme_load_index(int i)
{
    theme_t t;

    if (i < 0 || i >= listed())
        return -1;
    if (ap_theme_load(i, s_names[i], &t) != 0)
        return -2;                   /* (the file went, or the store is unavailable: unchanged) */
    g_current = t;
    g_have_current = 1;
    return 0;
}

int theme_load_named(const char *name)
{
    int i;

    if (name == 0)
        return -1;
    i = theme_index(name);
    if (i < 0)
        return -2;   /* unknown theme name — current theme left untouched */
    return theme_load_index(i);
}

int theme_validate(const theme_t *t)
{
    int i;

    if (t == 0)
        return -1;

    /* Non-empty name: the settings UI lists themes by name. */
    if (t->name[0] == '\0')
        return -2;
    for (i = 0; i < THEME_NAME_MAX; i++)
        if (t->name[i] == '\0')
            break;
    if (i == THEME_NAME_MAX)
        return -2;   /* not NUL-terminated */

    /* Readability floor: text and accent must differ from the background. */
    if (t->text == t->background)
        return -3;
    if (t->accent == t->background)
        return -4;

    /* Semantic colors must be distinguishable from each other. */
    if (t->danger == t->success)
        return -5;

    if (t->spacing < 0 || t->spacing > 64)
        return -6;
    if (t->radius < 0 || t->radius > 64)
        return -7;
    if (t->font_id < 0)
        return -8;

    return 0;
}
