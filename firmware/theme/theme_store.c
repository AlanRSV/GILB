/*
 * theme_store.c -- the themes the picker offers: the built-ins (themes_builtin.c), then each
 * RECHORD/THEMES/<name>.txt on Internal (theme_file.c's key=value format; Echo Library's theme
 * editor writes them). In the viz overlay with the built-in table and the parser (startup/ap_ovl.c
 * ap_theme_scan / ap_theme_load), so main memory keeps only the current theme and the names
 * (theme.c).
 *
 * A file theme starts from Classic and overrides what the file names: a file of one line is a
 * whole theme, and a typo costs that line only. Its name is the file's (without .txt), cut to
 * THEME_NAME_MAX - 1; a file named as a built-in, a second file of the same name, or more than
 * THEME_MAX themes in all are left out. Files are listed by name.
 */
#include <stdio.h>
#include <string.h>

#include "theme.h"
#include "theme_priv.h"
#include "services/fs.h"

static struct {
    char (*names)[THEME_NAME_MAX];
    int n, max, builtins;
} S;

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int name_cmp(const char *a, const char *b)   /* ASCII case-insensitive */
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
        a++, b++;
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static int found(const char *name, int is_dir)
{
    size_t len = strlen(name), k;
    char stem[THEME_NAME_MAX];
    int at;

    if (is_dir || len < 5 || name[len - 4] != '.' || lower(name[len - 3]) != 't' || lower(name[len - 2]) != 'x' ||
        lower(name[len - 1]) != 't' || name[0] == '.')
        return 0;                        /* (not a .txt; "._x.txt": a Mac's resource file) */
    k = len - 4 < sizeof stem - 1 ? len - 4 : sizeof stem - 1;
    memcpy(stem, name, k);
    stem[k] = 0;
    for (at = 0; at < S.n; at++)
        if (!name_cmp(S.names[at], stem))
            return 0;                    /* (a built-in's name, or there already) */
    if (S.n >= S.max)
        return 1;
    for (at = S.n; at > S.builtins && name_cmp(S.names[at - 1], stem) > 0; at--)
        memcpy(S.names[at], S.names[at - 1], THEME_NAME_MAX);   /* (by name) */
    memcpy(S.names[at], stem, THEME_NAME_MAX);
    S.n++;
    return 0;
}

int theme_store_scan(char (*names)[THEME_NAME_MAX], int max)
{
    S.names = names;
    S.max = max;
    S.n = 0;
    for (int i = 0; i < theme_builtin_count() && S.n < max; i++)
        snprintf(names[S.n++], THEME_NAME_MAX, "%s", theme_builtin_get(i)->name);
    S.builtins = S.n;
    (void)fs_list_dir_each(THEME_DIR, found);   /* (no folder: the built-ins alone) */
    return S.n;
}

int theme_store_load(int i, const char *name, theme_t *out)
{
    char path[64], text[1024];
    fs_file_t *f;
    long n;

    if (i < theme_builtin_count()) {
        *out = *theme_builtin_get(i);
        return 0;
    }
    snprintf(path, sizeof path, THEME_DIR "/%s.txt", name);
    if (!(f = fs_open_read(path)))
        return -1;
    n = fs_read(f, text, sizeof text - 1);
    fs_close(f);
    if (n < 0)
        return -1;
    text[n] = 0;                         /* (a longer file: its first 1023 bytes) */
    *out = *theme_builtin_get(0);
    (void)theme_file_parse(text, out);
    snprintf(out->name, sizeof out->name, "%s", name);
    return 0;
}
