/*
 * theme_priv.h — private to firmware/theme/ (built-in table access).
 *
 * Other modules must NOT include this file: they consume theme.h only.
 * It exists so theme.c can enumerate the .c data table without exposing
 * the table layout to the rest of the firmware.
 */
#ifndef THEME_PRIV_H
#define THEME_PRIV_H

#include "theme.h"

int           theme_builtin_count(void);
const theme_t *theme_builtin_get(int i);   /* NULL if i out of range */

/* theme_store.c (the device: in an overlay): every theme's name into names (the built-ins,
 * then RECHORD/THEMES/'s files); returns how many. Theme i (named name) into *out: 0, or < 0
 * (its file could not be read). */
int theme_store_scan(char (*names)[THEME_NAME_MAX], int max);
int theme_store_load(int i, const char *name, theme_t *out);

/* What theme.c calls: the store through its overlay's gate (startup/ap_ovl.c; < 0 when the
 * overlay cannot be had), or the store itself (the host test). */
int ap_theme_scan(char (*names)[THEME_NAME_MAX], int max);
int ap_theme_load(int i, const char *name, theme_t *out);

#endif /* THEME_PRIV_H */
