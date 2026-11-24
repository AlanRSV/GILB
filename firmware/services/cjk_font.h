/*
 * cjk_font.h -- Japanese, Chinese and Korean glyphs from GILB/CJK.BIN (services/cjk_font.c).
 */
#ifndef RECHORD_CJK_FONT_H
#define RECHORD_CJK_FONT_H

#include <stdint.h>

/* Hands drivers/text.c the reader and cache (RAM no one else uses: len / 36 glyphs, up to 128). */
void cjk_font_init(void *cache, uint32_t len);

#endif /* RECHORD_CJK_FONT_H */
