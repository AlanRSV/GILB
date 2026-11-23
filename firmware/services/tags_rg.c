/*
 * tags_rg.c -- the player's tag reader again, with ReplayGain (services/tags.h TAGS_RG): services/tags.c
 * built a third time, into the npx overlay (firmware_ap.ld) for audio/rg_read.c. The resident build
 * (title, artist, album, track) is unchanged.
 */
#define TAGS_RG 1
#include "tags.c"
