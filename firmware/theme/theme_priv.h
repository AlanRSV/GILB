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
int theme_store_scan(int retro, char (*names)[THEME_NAME_MAX], int max);
int theme_store_load(int retro, int i, const char *name, theme_t *out);

/* What theme.c calls: the store through its overlay's gate (startup/ap_ovl.c; < 0 when the
 * overlay cannot be had), or the store itself (the host test). */
int ap_theme_scan(int retro, char (*names)[THEME_NAME_MAX], int max);
int ap_theme_load(int retro, int i, const char *name, theme_t *out);

/* Look: Retro's built-ins (themes_builtin.c): name and its three colours (RGB565). */
typedef struct { const char *name; uint16_t glass, ink, highlight; } theme_retro_t;
int theme_retro_count(void);
const theme_retro_t *theme_retro_get(int i);
/* A whole Retro theme from its three colours (theme_store.c). */
void theme_retro_derive(uint16_t glass, uint16_t ink, uint16_t highlight, theme_t *out);

#endif /* THEME_PRIV_H */
