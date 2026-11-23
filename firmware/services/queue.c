/*
 * queue.c -- the play queue (queue.h). The folder listings come from dirlist.c
 * (the browser's view is reloaded by the browser afterwards).
 */
#include <string.h>

#include "queue.h"
#include "dirlist.h"
#include "drivers/clock.h"

typedef struct {
    fs_ref_t ref;
    uint8_t kind;
} q_ent_t;

static q_ent_t s_q[Q_MAX];
static uint16_t s_order[Q_MAX];              /* play order -> queue entry */
static int s_n, s_mode, s_stream, s_pos;
static uint32_t s_rng = 0x9E3779B9u;

static void add_file(int i, int *incomplete)
{
    if (s_n == Q_MAX) {
        *incomplete = 1;
        return;
    }
    s_q[s_n].ref = *dirlist_ref(i);
    s_q[s_n].kind = (uint8_t)dirlist_kind_at(i);
    s_order[s_n] = (uint16_t)s_n;
    s_n++;
}

void queue_clear(void)
{
    s_n = s_stream = s_pos = 0;
}

int queue_from_folder(const char *path, const fs_ref_t *folder, const fs_ref_t *start, int *incomplete)
{
    int page = 0, more = 1, first = 0;

    queue_clear();
    *incomplete = 0;
    while (more && s_n < Q_MAX) {
        int n = dirlist_load(path, folder, page++, &more);

        if (n < 0)
            return n;
        for (int i = 0; i < n; i++) {
            if (dirlist_is_dir(i))
                continue;
            if (start && !memcmp(dirlist_ref(i), start, sizeof *start))
                first = s_n;
            add_file(i, incomplete);
        }
    }
    if (more)
        *incomplete = 1;
    s_stream = s_pos = first;
    if (s_mode == Q_SHUFFLE)
        queue_set_mode(Q_SHUFFLE);
    return 0;
}

int queue_from_list(int n, int start, int (*get)(int i, fs_ref_t *r, int *kind))
{
    int first = 0;

    queue_clear();
    for (int i = 0; i < n && s_n < Q_MAX; i++) {
        int kind;

        if (get(i, &s_q[s_n].ref, &kind))
            continue;
        if (i <= start)
            first = s_n;                     /* (the start, or the last track before it) */
        s_q[s_n].kind = (uint8_t)kind;
        s_order[s_n] = (uint16_t)s_n;
        s_n++;
    }
    s_stream = s_pos = first;
    if (s_mode == Q_SHUFFLE)
        queue_set_mode(Q_SHUFFLE);
    return s_n ? 0 : FS_ERR_OPEN;
}

/* Depth first, sorted: a folder's files, then each of its subfolders. Pending
 * folders wait on a stack (pushed in reverse so they come off in order). */
#define PENDING 128

int queue_from_tree(const char *path, const fs_ref_t *folder, int *incomplete)
{
    static struct { fs_ref_t ref; uint8_t depth; } stack[PENDING];
    int sp = 0, first = 1;
    fs_ref_t cur;
    uint8_t depth = 0;

    queue_clear();
    *incomplete = 0;
    for (;;) {
        int page = 0, more = 1;

        while (more) {
            int n = dirlist_load(first ? path : NULL, first ? folder : &cur, page++, &more), dirs = 0;

            if (n < 0)
                return first ? n : 0;
            for (int i = 0; i < n; i++)
                if (!dirlist_is_dir(i))
                    add_file(i, incomplete);
                else
                    dirs++;
            if (dirs && depth >= Q_DEPTH)
                *incomplete = 1;                 /* deeper folders are not taken */
            else
                for (int i = n - 1; i >= 0; i--)
                    if (dirlist_is_dir(i)) {
                        if (sp == PENDING) {
                            *incomplete = 1;
                            break;
                        }
                        stack[sp].ref = *dirlist_ref(i);
                        stack[sp++].depth = (uint8_t)(depth + 1u);
                    }
        }
        first = 0;
        if (!sp || s_n == Q_MAX)
            break;
        sp--;
        cur = stack[sp].ref;
        depth = stack[sp].depth;
    }
    if (sp)
        *incomplete = 1;
    s_stream = s_pos = 0;
    if (s_mode == Q_SHUFFLE)
        queue_set_mode(Q_SHUFFLE);
    return 0;
}

int queue_count(void) { return s_n; }
int queue_mode(void) { return s_mode; }

const char *queue_mode_name(int mode)
{
    static const char *const k[Q_MODES] = { "normal", "shuffle", "repeat track", "repeat all" };

    return mode >= 0 && mode < Q_MODES ? k[mode] : "?";
}

static uint32_t rnd(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

void queue_set_mode(int mode)
{
    int cur = s_n ? s_order[s_pos] : 0;

    if (mode < 0 || mode >= Q_MODES)
        return;
    s_mode = mode;
    for (int i = 0; i < s_n; i++)
        s_order[i] = (uint16_t)i;
    if (mode == Q_SHUFFLE && s_n > 1) {
        s_rng ^= time_us();
        s_order[0] = (uint16_t)cur;             /* the current track stays first */
        s_order[cur] = 0;
        for (int i = s_n - 1; i > 1; i--) {     /* Fisher-Yates over the rest */
            int j = 1 + (int)(rnd() % (uint32_t)i);
            uint16_t t = s_order[i];

            s_order[i] = s_order[j];
            s_order[j] = t;
        }
        s_pos = 0;
    } else {
        s_pos = cur;
    }
    s_stream = s_pos + 1;                       /* the engine has the current one already */
}

int queue_take(fs_ref_t *ref, int *kind, int *pos)
{
    int p = s_stream;

    if (!s_n)
        return -1;
    if (s_mode == Q_REPEAT_ONE)
        p = s_pos;                              /* the same track again, gapless */
    else if (p >= s_n) {
        if (s_mode != Q_REPEAT_ALL)
            return -1;
        p = 0;
    }
    *ref = s_q[s_order[p]].ref;
    *kind = s_q[s_order[p]].kind;
    *pos = p;
    s_stream = p + 1;
    return 0;
}

void queue_seek(int pos)
{
    if (pos >= 0 && pos < s_n)
        s_stream = s_pos = pos;
}

void queue_heard(int pos)
{
    if (pos >= 0 && pos < s_n)
        s_pos = pos;
}

int queue_pos(void) { return s_pos; }

int queue_next_pos(void)
{
    if (s_pos + 1 < s_n)
        return s_pos + 1;
    return s_mode == Q_REPEAT_ALL && s_n ? 0 : -1;
}

int queue_prev_pos(void)
{
    if (s_pos > 0)
        return s_pos - 1;
    return s_mode == Q_REPEAT_ALL && s_n ? s_n - 1 : -1;
}

const fs_ref_t *queue_ref(int pos)
{
    return pos >= 0 && pos < s_n ? &s_q[s_order[pos]].ref : NULL;
}

/* ---- the queue on the disk: QUEUE.BIN ------------------------------------------------------------
 * Saved at power-off, screen off and a USB session, read back by a "Last track" start, so the same
 * queue (a library list, a playlist, shuffle's order) plays on from the same track and time. These
 * functions are in the npx overlay (firmware_ap.ld, by section): startup/ap_ovl.c ap_npx_queue.
 * A saved file reference is checked against its directory entry before it is used again
 * (fs_ref_check): tracks the computer deleted or replaced meanwhile are dropped. */
#include "fs.h"
#include "log.h"
#include "player.h"

#define QF_PATH  "/Internal/RECHORD/QUEUE.BIN"
#define QF_MAGIC 0x31455551u                 /* "QUE1" */
#define QK_BAD   0x80u                       /* (kind's top bit: a reference that no longer holds) */

static uint8_t s_lost;                       /* qf_check: the heard track was among those dropped */

typedef struct {
    uint32_t magic, n, pos, ms;
    uint8_t mode, tree, ent, rsv;
    char src[152];
} qf_hdr_t;

int player_src_get(char *out, unsigned long n);       /* player/player_audio.c (this overlay) */
void player_src_set(const char *src, int tree);

static int qf_save(void)
{
    qf_hdr_t h;
    fs_file_t *f;
    int ps = player_get_state(), ok;

    if (!s_n)
        return -1;
    memset(&h, 0, sizeof h);
    h.magic = QF_MAGIC;
    h.n = (uint32_t)s_n;
    h.pos = (uint32_t)s_pos;
    h.ms = ps == PLAYER_PLAYING || ps == PLAYER_PAUSED ? (uint32_t)player_get_position_ms() : 0u;
    h.mode = (uint8_t)s_mode;
    h.tree = (uint8_t)player_src_get(h.src, sizeof h.src);
    h.ent = (uint8_t)sizeof s_q[0];
    if (!(f = fs_open_write(QF_PATH)))
        return -2;
    ok = fs_write(f, &h, sizeof h) == (long)sizeof h &&
         fs_write(f, s_q, (unsigned long)s_n * sizeof s_q[0]) == (long)((unsigned long)s_n * sizeof s_q[0]) &&
         fs_write(f, s_order, (unsigned long)s_n * sizeof s_order[0]) == (long)((unsigned long)s_n * sizeof s_order[0]);
    fs_close(f);
    return ok ? 0 : -3;
}

static long read_all(fs_file_t *f, void *buf, unsigned long n)
{
    unsigned long got = 0;
    long r;

    while (got < n && (r = fs_read(f, (uint8_t *)buf + got, n - got)) > 0)
        got += (unsigned long)r;
    return (long)got;
}

/* Drops the tracks whose file is no longer what the reference says; the heard position moves to the
 * next track kept. Returns the number dropped. */
static int qf_check(void)
{
    int bad = 0, np = 0, newpos = -1, w = 0;

    for (int i = 0; i < s_n; i++)
        if (fs_ref_check(&s_q[i].ref) != 0) {
            s_q[i].kind |= QK_BAD;
            bad++;
        }
    s_lost = s_n && (s_q[s_order[s_pos]].kind & QK_BAD);
    if (!bad)
        return 0;
    for (int p = 0; p < s_n; p++) {          /* the order over the kept entries, renumbered */
        int e = s_order[p], before = 0;

        if (s_q[e].kind & QK_BAD)
            continue;
        if (p >= s_pos && newpos < 0)
            newpos = np;
        for (int i = 0; i < e; i++)
            before += !(s_q[i].kind & QK_BAD);
        s_order[np++] = (uint16_t)before;
    }
    for (int i = 0; i < s_n; i++)
        if (!(s_q[i].kind & QK_BAD))
            s_q[w++] = s_q[i];
    s_n = w;
    s_pos = s_stream = newpos >= 0 ? newpos : 0;
    return bad;
}

static long qf_load(void)
{
    qf_hdr_t h;
    fs_file_t *f = fs_open_read(QF_PATH);
    uint32_t t0 = time_ms();
    long ms;
    int ok, bad;

    if (!f)
        return -1;
    ok = read_all(f, &h, sizeof h) == (long)sizeof h && h.magic == QF_MAGIC && h.n && h.n <= Q_MAX &&
         h.ent == sizeof s_q[0] && h.pos < h.n && h.mode < Q_MODES;
    if (ok) {
        queue_clear();
        ok = read_all(f, s_q, h.n * sizeof s_q[0]) == (long)(h.n * sizeof s_q[0]) &&
             read_all(f, s_order, h.n * sizeof s_order[0]) == (long)(h.n * sizeof s_order[0]);
        for (uint32_t i = 0; ok && i < h.n; i++)
            ok = s_order[i] < h.n && !(s_q[i].kind & QK_BAD);
    }
    fs_close(f);
    if (!ok) {
        queue_clear();
        log_printf("queue: %s not usable", QF_PATH);
        return -2;
    }
    s_n = (int)h.n;
    s_pos = s_stream = (int)h.pos;
    s_mode = h.mode;
    bad = qf_check();
    if (!s_n)
        return -3;
    ms = s_lost ? 0 : (long)h.ms;
    h.src[sizeof h.src - 1] = 0;
    player_src_set(h.src, h.tree);
    log_printf("queue: %d tracks restored (%d gone), track %d at %ld ms, %s, %u ms", s_n, bad, s_pos + 1, ms,
               queue_mode_name(s_mode), (unsigned)(time_ms() - t0));
    return ms;
}

/* The npx overlay's entry (np_extra.h): 0 save, 1 load (the time to resume at, or < 0), 2 check the
 * queue in memory (after the computer wrote to the disk). */
long npx_queue(int op)
{
    if (op == 0) {
        int r = qf_save();

        if (r != -1)
            log_printf("queue: saved %d tracks, track %d (%d)", s_n, s_pos + 1, r);
        return r;
    }
    if (op == 1)
        return qf_load();
    {
        int bad = qf_check();

        if (bad)
            log_printf("queue: %d tracks gone after the USB session, %d left", bad, s_n);
        return bad;
    }
}
