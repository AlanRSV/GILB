/* global_data.h -- the music renderer's stand-in: mus.c only reads _g->mus_playing. */
#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H
typedef struct { int mus_playing; } host_globals_t;
extern host_globals_t g_host;
#define _g (&g_host)
#endif
