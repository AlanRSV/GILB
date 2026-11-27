/* w_wad.h -- the music renderer's stand-in: the instruments (GENMIDI) and each song's score, as
   "addresses" that extMemory.h reads from the WAD loaded in memory. */
#ifndef W_WAD_H
#define W_WAD_H
#include <stdint.h>
#include "mus.h"
typedef struct {
    const genmidi_instr_t *instruments;
    uint32_t *musicAddrs;
} host_wad_t;
extern host_wad_t *p_wad_immutable_flash_data;
#endif
