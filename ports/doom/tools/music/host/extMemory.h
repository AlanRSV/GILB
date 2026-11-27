/* extMemory.h -- the music renderer's stand-in: an address is an offset into the WAD in memory. */
#ifndef SRC_EXTMEMORY_H_
#define SRC_EXTMEMORY_H_
#include <stdint.h>
#include <string.h>
extern const uint8_t *g_wad;
extern uint32_t g_wad_len, g_at;
static inline void extMemSetCurrentAddress(uint32_t a) { g_at = a; }
static inline void *extMemGetDataFromCurrentAddress(void *d, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
        ((uint8_t *)d)[i] = g_at + i < g_wad_len ? g_wad[g_at + i] : 0;
    g_at += n;
    return d;
}
#endif
