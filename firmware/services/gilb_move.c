/*
 * gilb_move.c -- the firmware's own files under its new name (the user, 2026-11-24: ReChord's fork
 * is GILB): RECHORD.CFG becomes GILB.CFG and a volume's RECHORD/ folder (the library's index,
 * playlists, the queue, the splash, the themes) becomes GILB/. Run once a volume shows the old
 * name: Internal at boot before the settings and the splash are read (startup/ap_ui.c), the card
 * when the library first looks for its folder there (services/lib_job.c lib_data_dir). In the viz
 * overlay (startup/ap_ovl.c ap_gilb_move): main memory only checks whether there is anything to do.
 *
 * A folder rename keeps every file where it is on the disk (the index's and the queue's references
 * stay good). When GILB/ is already there (an older Echo Library wrote to RECHORD/ after the move,
 * or the other way round) each entry GILB/ lacks is moved into it and the rest are left in place.
 */
#include <stdio.h>
#include <string.h>

#include "fs.h"
#include "log.h"

static char s_name[96];                  /* the entry found by pick() */
static unsigned s_skip, s_seen;

static int pick(const char *name, int is_dir)
{
    (void)is_dir;
    if (s_seen++ < s_skip)
        return 0;
    snprintf(s_name, sizeof s_name, "%s", name);
    return 1;
}

int gilb_move(int vol)
{
    const char *v = vol ? "SD" : "Internal";
    char from[24], to[24], a[128], b[128];
    int r = 0, moved = 0;

    if (!vol && !fs_exists("/Internal/GILB.CFG") && fs_exists("/Internal/RECHORD.CFG")) {
        r = fs_rename("/Internal/RECHORD.CFG", "/Internal/GILB.CFG");
        log_printf("gilb: RECHORD.CFG -> GILB.CFG: %d", r);
    }
    snprintf(from, sizeof from, "/%s/RECHORD", v);
    snprintf(to, sizeof to, "/%s/GILB", v);
    if (!fs_exists(from))
        return r;
    if (!fs_exists(to)) {
        r = fs_rename(from, to);
        log_printf("gilb: %s -> %s: %d", from, to, r);
        return r;
    }
    s_skip = 0;
    for (int guard = 0; guard < 256; guard++) {   /* both there: what GILB/ lacks goes into it */
        s_name[0] = 0;
        s_seen = 0;
        if (fs_list_dir_each(from, pick) < 0 || !s_name[0])
            break;
        snprintf(a, sizeof a, "%s/%s", from, s_name);
        snprintf(b, sizeof b, "%s/%s", to, s_name);
        if (fs_exists(b) || fs_rename(a, b))
            s_skip++;                    /* (GILB/ has its own: this one stays) */
        else
            moved++;
    }
    log_printf("gilb: %s and %s both there: %d moved, %u left", from, to, moved, s_skip);
    return 0;
}
