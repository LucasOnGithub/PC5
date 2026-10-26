/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5 updates itself: the newest release's app zip (PPSA11018.zip) from GitHub,
 * checked against the size and SHA-256 GitHub lists for it, unpacked in a work
 * folder on the same drive as PC5's folder and checked again (each file's CRC-32),
 * then put in place of PC5's files inside that folder. The folder itself stays
 * where it is, so the console keeps finding PC5 where it was installed.
 *
 * Each new file is renamed over the old one, so no file is ever missing, and
 * eboot.bin goes last: if the console stops part way, the PC5 that starts next
 * (the old one until eboot.bin is replaced, which has this code too) finishes the
 * job from the journal in the work folder. Then the files of the release's folders
 * that it no longer has are deleted; what is at the top of PC5's folder and not in
 * the release (a player's own files) stays.
 *
 * PC5's folder is the one on the console's drives (in homebrew, etaHEN/games or at
 * the top of a USB or extended drive) whose eboot.bin is the running one's; when
 * there are several, PC5 doesn't guess. The work is done on a thread of its own;
 * the screens show its progress. */
#ifndef PC5_UPGRADE_H
#define PC5_UPGRADE_H
#include "pc5.h"
#include "pc5_cover.h"

#include <stdatomic.h>

typedef enum Pc5UpgradeStep {
    PC5_UPGRADE_IDLE,
    PC5_UPGRADE_FINDING,       /* asking GitHub, finding PC5's folder */
    PC5_UPGRADE_DOWNLOADING,   /* done/total: bytes of the zip */
    PC5_UPGRADE_UNPACKING,     /* done/total: bytes of the files */
    PC5_UPGRADE_REPLACING,     /* done/total: files; can't be stopped from here on */
    PC5_UPGRADE_TIDYING,       /* deleting the old files */
    PC5_UPGRADE_DONE,
    PC5_UPGRADE_FAILED,        /* error says why; PC5's folder is as it was unless partial */
    PC5_UPGRADE_CANCELLED,
} Pc5UpgradeStep;

typedef struct Pc5UpgradeConfig {
    Pc5Http http;               /* get for the release, download for the zip */
    const char *app;            /* where the running PC5's files are seen (its app0) */
    const char *root;           /* PC5's library (/data/prospero-win): root/pc5/update is the work folder on /data */
    const char *const *drives;  /* where PC5's folder may be, NULL-terminated; NULL: the console's */
    const char *user;           /* the console's copies of the app's sce_sys (/user), NULL for none */
    void (*log)(void *context, const char *line);
    void *log_context;
} Pc5UpgradeConfig;

typedef struct Pc5Upgrade {
    Pc5UpgradeConfig config;
    atomic_int step;            /* Pc5UpgradeStep */
    atomic_int cancel;          /* asked to stop (honoured until PC5_UPGRADE_REPLACING) */
    atomic_uint_fast64_t done, total;
    /* Set by the worker before the step that shows them (read them after loading step). */
    char version[32];           /* the version being installed (from DOWNLOADING) */
    char folder[PC5_PATH];      /* PC5's folder (from DOWNLOADING) */
    char error[PC5_ERROR];      /* FAILED */
    int partial;                /* FAILED while files were put in place: the next start finishes it */
} Pc5Upgrade;

/* The console's drives, in the order PC5's folder is looked for. */
extern const char *const pc5_upgrade_drives[];

/* Starts the worker thread. -1 (error set, step FAILED) when it couldn't. */
int pc5_upgrade_start(Pc5Upgrade *upgrade, const Pc5UpgradeConfig *config);
/* The work itself, on the caller's thread: what the worker runs (and the tests). */
void pc5_upgrade_run(Pc5Upgrade *upgrade);
void pc5_upgrade_cancel(Pc5Upgrade *upgrade);
/* 1 while the worker is busy (between start and DONE, FAILED or CANCELLED). */
int pc5_upgrade_busy(const Pc5Upgrade *upgrade);

/* When PC5 opens, before anything reads the app's files: finishes putting files in
 * place if the console stopped while that was being done, and deletes what an
 * update left in its work folders. 1 when it put eboot.bin in place, so the
 * running PC5 is the old one and should close (finished: the new version). */
int pc5_upgrade_tidy(const Pc5UpgradeConfig *config, char *finished, size_t size);

/* PC5's folder: the folder on config's drives whose eboot.bin is the same as
 * config->app's. 0; -1 when there is none; -2 when there are several (folder and
 * other name two of them). */
int pc5_upgrade_find(const Pc5UpgradeConfig *config, char *folder, size_t size, char *other, size_t other_size);

/* Unpacks the zip at path into folder (made if missing): the files under prefix
 * (which ends in '/': "PPSA11018/"), without it, each checked by its CRC-32; the files and folders
 * made 0777. done and total (bytes, both may be NULL) follow it; cancel (may be NULL)
 * stops it; files larger in all than room aren't unpacked. names (may be NULL) gets
 * the names unpacked, without the prefix, a line each (folders end in '/').
 * 0, or -1 with error set. */
int pc5_unzip(const char *path, const char *prefix, const char *folder, atomic_uint_fast64_t *done,
              atomic_uint_fast64_t *total, const atomic_int *cancel, uint64_t room, Pc5Buf *names, char *error,
              size_t error_size);

#endif
