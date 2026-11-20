/*
 * lyrics.h -- a track's lyrics for Now Playing's Lyrics view: found in its tags (FLAC Vorbis
 * comments LYRICS / UNSYNCEDLYRICS, ID3v2 USLT in MP3s and in WAV's "id3 " chunk) or in a .lrc
 * file beside it, then cut into lines. Timed (LRC) lyrics keep each line's time; plain lyrics
 * are lines only (the view spreads them over the song).
 *
 * No malloc: the caller owns the lyrics_t (in the npx overlay's resident .bss). Reads go through
 * the same read-at callback as services/tags.h. Pure code: tested on the host
 * (services/tests/test_lyrics.c), linked into the npx overlay on the device.
 */
#ifndef RECHORD_LYRICS_H
#define RECHORD_LYRICS_H

#include <stdint.h>

#include "services/tags.h"

#define LYR_TEXT  4096u                /* bytes of lyrics kept (a song's are 1..3 KB) */
#define LYR_LINES 192u                 /* lines kept (a repeated chorus counts once per time) */

typedef struct {
    uint16_t n;                        /* lines */
    uint8_t synced;                    /* 1: each line has its time (ds[]) */
    uint16_t at[LYR_LINES];            /* line i's text: text + at[i], NUL-terminated */
    uint16_t ds[LYR_LINES];            /* its time, tenths of a second (synced only) */
    char text[LYR_TEXT];
} lyrics_t;

/* The raw lyrics (UTF-8) in the tags of a file of the given kind (TAGS_FLAC, TAGS_MP3, TAGS_WAV;
 * others: none) into out[0..cap-1], NUL-terminated. Returns their length, 0 when there are none. */
uint32_t lyrics_tag(tags_read_at_t rd, void *u, uint32_t size, int kind, char *out, uint32_t cap);

/* Cut l->text[0..len) (UTF-8; LRC or plain) into lines, in place. LRC: lines without a time and
 * [ar:]/[ti:]-style headers are dropped, a line with several times is listed at each, [offset:]
 * applies, <mm:ss.xx> word times are removed, lines are sorted by time. Plain: empty lines go. */
void lyrics_parse(lyrics_t *l, uint32_t len);

/* The line being sung at ms (synced): the last one whose time has come, -1 before the first. */
int lyrics_line(const lyrics_t *l, uint32_t ms);

#endif
