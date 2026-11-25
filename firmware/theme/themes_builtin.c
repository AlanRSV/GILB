/*
 * themes_builtin.c — built-in ReChord themes as .c data tables.
 *
 * WHY data in .c: the build-from-source rule (docs/rewrite/architecture.md §1)
 * wants every byte rebuildable from source; .c tables need no asset pipeline,
 * compile on host and target alike, and can be `const` (flash-resident on the
 * Cortex-M3). This file is the ONLY place in the firmware where color values
 * exist — everything else asks theme_get().
 *
 * The values are DESIGN.md 5.1's tokens (8-bit sRGB, in the comments), the same
 * as tools/ui_mockups.py THEMES in the project repo, quantised to RGB565 as
 * ((r >> 3) << 11 | (g >> 2) << 5 | b >> 3). tests/ui_tokens_check.py there
 * checks every one against the mockups.
 *
 *   Classic  — dark slate, the default.
 *   Paper    — warm light/sepia for daylight (no pure white/black pair).
 *   Night    — very dim, for bedside use.
 *   Contrast — black, white and yellow, WCAG ratios 13..21.
 *   Pink     — deep plum with a rose accent (the user, 2026-11-24).
 *
 * Look: Retro's built-ins (theme_retro_get) are three colours each, glass (the background),
 * ink (text and lines) and highlight (the cursor's row); theme_store.c works out the rest:
 *   LCD       — navy on pale blue-grey, an old LCD (the user, 2026-11-24).
 *   Amber     — dark brown on an amber backlight.
 *   Green LCD — dark olive on pea-green glass.
 *   Mono      — near-black on light grey, an unlit monochrome LCD.
 *
 * On the device this table is in the viz overlay with theme_store.c (main memory keeps only
 * the current theme: theme.c).
 */
#include <string.h>

#include "theme.h"
#include "theme_priv.h"

static const theme_t builtin_themes[] =
{
    {
        .name = "Classic",
        .background = 0x1083,   /* 121218 */
        .surface    = 0x1905,   /* 1E2029 */
        .text       = 0xEF7E,   /* ECEEF2 */
        .text_dim   = 0x94B4,   /* 9296A0 */
        .accent     = 0x44FF,   /* 409CFF */
        .danger     = 0xFA8A,   /* FF5252 */
        .success    = 0x56AF,   /* 52D678 */
        .header_bg  = 0x1083,   /* 121218 */
        .divider    = 0x2987,   /* 2E313C */
        .text_off   = 0x5B0D,   /* 5C606A */
        .on_accent  = 0xFFFF,   /* FFFFFF */
        .sel_bg     = 0x2B37,   /* 2A66BE */
        .sel_text   = 0xFFFF,   /* FFFFFF */
        .value      = 0x6DBF,   /* 6FB5FF */
        .keybar_bg  = 0x18E4,   /* 1A1C24 */
        .key_text   = 0xD6BB,   /* D4D7DE */
        .key_hold   = 0x7C11,   /* 7C808A */
        .key_press  = 0x2B77,   /* 2D6FB8 */
        .bar_track  = 0x2987,   /* 2E313C */
        .bar_fill   = 0x44FF,   /* 409CFF */
        .badge_bg   = 0x2987,   /* 2E313C */
        .badge_text = 0xE73D,   /* E2E4EA */
        .hires      = 0xED89,   /* E8B04B */
        .warn       = 0xFD84,   /* FFB020 */
        .scroll     = 0x5B0D,   /* 5C606A */
        .spacing = 8, .radius = 6, .font_id = 0,
    },
    {
        .name = "Paper",
        .background = 0xF79C,   /* F6F0E4 */
        .surface    = 0xFFFE,   /* FFFCF4 */
        .text       = 0x3144,   /* 302A20 */
        .text_dim   = 0x7B8C,   /* 7A7060 */
        .accent     = 0x9AC2,   /* 9E5A14 */
        .danger     = 0xB145,   /* B02A2A */
        .success    = 0x2BE8,   /* 2E7C46 */
        .header_bg  = 0xEF3A,   /* EDE5D4 */
        .divider    = 0xD656,   /* D3C8B4 */
        .text_off   = 0xB552,   /* B3A994 */
        .on_accent  = 0xFFFF,   /* FFFFFF */
        .sel_bg     = 0x9AC2,   /* 9E5A14 */
        .sel_text   = 0xFFFF,   /* FFFFFF */
        .value      = 0x9AC2,   /* 9E5A14 */
        .keybar_bg  = 0xEF3A,   /* EDE5D4 */
        .key_text   = 0x3144,   /* 302A20 */
        .key_hold   = 0x8C0D,   /* 8A806E */
        .key_press  = 0xDDD1,   /* D9B98F */
        .bar_track  = 0xDE97,   /* DCD2BF */
        .bar_fill   = 0x9AC2,   /* 9E5A14 */
        .badge_bg   = 0xE6D8,   /* E3D9C5 */
        .badge_text = 0x3144,   /* 302A20 */
        .hires      = 0x9B40,   /* 9A6A00 */
        .warn       = 0xAB00,   /* A86200 */
        .scroll     = 0xB552,   /* B3A994 */
        .spacing = 10, .radius = 2, .font_id = 0,
    },
    {
        .name = "Night",
        .background = 0x0841,   /* 08080C */
        .surface    = 0x10A3,   /* 16161E */
        .text       = 0xAD56,   /* AAAAB2 */
        .text_dim   = 0x630D,   /* 62626E */
        .accent     = 0x23B6,   /* 2474B4 */
        .danger     = 0xB2AA,   /* B45656 */
        .success    = 0x44AD,   /* 46966A */
        .header_bg  = 0x0841,   /* 08080C */
        .divider    = 0x18E4,   /* 1C1C26 */
        .text_off   = 0x39E8,   /* 3C3C46 */
        .on_accent  = 0xDEDC,   /* D8D8E0 */
        .sel_bg     = 0x1A6F,   /* 1D4E78 */
        .sel_text   = 0xDEDC,   /* D8D8E0 */
        .value      = 0x4C78,   /* 4F8CC0 */
        .keybar_bg  = 0x0862,   /* 0E0E14 */
        .key_text   = 0x8C52,   /* 8A8A94 */
        .key_hold   = 0x630D,   /* 62626E */
        .key_press  = 0x1A6F,   /* 1D4E78 */
        .bar_track  = 0x18E4,   /* 1C1C26 */
        .bar_fill   = 0x23B6,   /* 2474B4 */
        .badge_bg   = 0x18E4,   /* 1C1C26 */
        .badge_text = 0x9CD4,   /* 9A9AA4 */
        .hires      = 0xA3C6,   /* A07A30 */
        .warn       = 0xABC5,   /* A87A2C */
        .scroll     = 0x39E8,   /* 3C3C46 */
        .spacing = 8, .radius = 6, .font_id = 0,
    },
    {
        .name = "Contrast",
        .background = 0x0000,   /* 000000 */
        .surface    = 0x0000,   /* 000000 */
        .text       = 0xFFFF,   /* FFFFFF */
        .text_dim   = 0xE73C,   /* E6E6E6 */
        .accent     = 0xFEA0,   /* FFD500 */
        .danger     = 0xFA8A,   /* FF5050 */
        .success    = 0x57F0,   /* 50FF80 */
        .header_bg  = 0x0000,   /* 000000 */
        .divider    = 0xFFFF,   /* FFFFFF */
        .text_off   = 0x8C71,   /* 8C8C8C */
        .on_accent  = 0x0000,   /* 000000 */
        .sel_bg     = 0xFEA0,   /* FFD500 */
        .sel_text   = 0x0000,   /* 000000 */
        .value      = 0xFEA0,   /* FFD500 */
        .keybar_bg  = 0x0000,   /* 000000 */
        .key_text   = 0xFFFF,   /* FFFFFF */
        .key_hold   = 0xCE59,   /* C8C8C8 */
        .key_press  = 0xFEA0,   /* FFD500 */
        .bar_track  = 0x5ACB,   /* 5A5A5A */
        .bar_fill   = 0xFEA0,   /* FFD500 */
        .badge_bg   = 0xFFFF,   /* FFFFFF */
        .badge_text = 0x0000,   /* 000000 */
        .hires      = 0xFEA0,   /* FFD500 */
        .warn       = 0xFD80,   /* FFB000 */
        .scroll     = 0xFFFF,   /* FFFFFF */
        .spacing = 8, .radius = 0, .font_id = 0,
    },
    {
        .name = "Pink",
        .background = 0x1883,   /* 1E1218 */
        .surface    = 0x28C4,   /* 2A1A23 */
        .text       = 0xFF5D,   /* F8E8EF */
        .text_dim   = 0xBCB3,   /* B8949F */
        .accent     = 0xFB75,   /* FF6FAE */
        .danger     = 0xFACB,   /* FF5A5A */
        .success    = 0x5EB3,   /* 5ED69A */
        .header_bg  = 0x1883,   /* 1E1218 */
        .divider    = 0x3926,   /* 3A2630 */
        .text_off   = 0x6AAC,   /* 6E5560 */
        .on_accent  = 0xFFFF,   /* FFFFFF */
        .sel_bg     = 0xC20F,   /* C2427E */
        .sel_text   = 0xFFFF,   /* FFFFFF */
        .value      = 0xFCF9,   /* FF9CC8 */
        .keybar_bg  = 0x20A3,   /* 26171F */
        .key_text   = 0xF6DC,   /* F0D8E3 */
        .key_hold   = 0xA411,   /* A0808C */
        .key_press  = 0xC20F,   /* C2427E */
        .bar_track  = 0x3926,   /* 3A2630 */
        .bar_fill   = 0xFB75,   /* FF6FAE */
        .badge_bg   = 0x3926,   /* 3A2630 */
        .badge_text = 0xFF5D,   /* F8E8EF */
        .hires      = 0xFE2D,   /* FFC56B */
        .warn       = 0xFD88,   /* FFB347 */
        .scroll     = 0x6AAC,   /* 6E5560 */
        .spacing = 8, .radius = 6, .font_id = 0,
    },
};

#define BUILTIN_COUNT ((int)(sizeof(builtin_themes) / sizeof(builtin_themes[0])))

int theme_builtin_count(void)
{
    return BUILTIN_COUNT;
}

const theme_t *theme_builtin_get(int i)
{
    if (i < 0 || i >= BUILTIN_COUNT)
        return 0;
    return &builtin_themes[i];
}

const theme_t *theme_builtin(int i)
{
    return theme_builtin_get(i);
}

static const theme_retro_t retro_themes[] = {
    { "LCD",       0xC69C, 0x218F, 0x218F },   /* C5D0E2, 23317F */
    { "Amber",     0xF547, 0x38E0, 0x38E0 },   /* F0A93A, 3B1D05 */
    { "Green LCD", 0x9DE1, 0x09C1, 0x09C1 },   /* 9BBC0F, 0F380F */
    { "Mono",      0xCE58, 0x1082, 0x1082 },   /* C8C8C0, 101010 */
};

int theme_retro_count(void)
{
    return (int)(sizeof retro_themes / sizeof retro_themes[0]);
}

const theme_retro_t *theme_retro_get(int i)
{
    return i >= 0 && i < theme_retro_count() ? &retro_themes[i] : 0;
}

/* a .. b, t/256 of the way (RGB565, rounded) */
static uint16_t mix(uint16_t a, uint16_t b, unsigned t)
{
    unsigned r = ((a >> 11) * (256u - t) + (b >> 11) * t + 128u) >> 8,
             g = (((a >> 5) & 63u) * (256u - t) + ((b >> 5) & 63u) * t + 128u) >> 8,
             bl = ((a & 31u) * (256u - t) + (b & 31u) * t + 128u) >> 8;

    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* rough brightness of an RGB565 colour, 0..~1000 */
static int luma(uint16_t c) { return (int)((c >> 11) * 2 * 299u + ((c >> 5) & 63u) * 587u + (c & 31u) * 2 * 114u) / 63; }

/* Retro's colours from three: the glass everywhere a background goes, the ink for text, lines and bars,
 * the highlight for the cursor's row; between them the dim text (17 % to the glass), the rules (5 %), the surface (7 %
 * to the ink), the unlit segments (62 % to the glass); the cursor row's text the glass or the ink,
 * whichever stands apart from the highlight. LCD's three give the LCD theme as it was. */
void theme_retro_derive(uint16_t glass, uint16_t ink, uint16_t highlight, theme_t *out)
{
    int dg = luma(glass) - luma(highlight), di = luma(ink) - luma(highlight);

    memset(out, 0, sizeof *out);
    out->background = out->header_bg = out->keybar_bg = out->bar_track = out->on_accent = out->badge_text = glass;
    out->sel_text = (dg < 0 ? -dg : dg) >= (di < 0 ? -di : di) ? glass : ink;
    out->text = out->accent = out->value = out->key_text = out->bar_fill = out->badge_bg = out->hires = ink;
    out->sel_bg = highlight;
    out->surface = mix(glass, ink, 18);
    out->text_dim = out->key_hold = mix(ink, glass, 44);
    out->divider = out->scroll = mix(ink, glass, 12);
    out->text_off = mix(ink, glass, 160);
    out->key_press = mix(glass, highlight, 64);
    out->danger = 0x88E5;                /* 8C1F2E */
    out->success = 0x1B47;               /* 1F6B3A */
    out->warn = 0x7A80;                  /* 7A5200 */
    out->spacing = 8;
}
