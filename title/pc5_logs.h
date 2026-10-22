/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: keeping prospero-win's logs (<root>/logs) only when something went
 * wrong. Each session (PC5's, or a game's) writes session-N.log while it
 * runs, since a crash can't write its log afterwards. When PC5 opens it
 * reads the sessions that have ended: one that ended well is deleted, one
 * with a problem is kept under a name that says when and what
 * (issue-2026-10-22_18-47_grand-theft-auto-iv.log). Logs older than seven
 * days go, whatever they hold. */
#ifndef PC5_LOGS_H
#define PC5_LOGS_H
#include "pc5.h"

enum { PC5_LOG_DAYS = 7 };

typedef enum Pc5LogProblem {
    PC5_LOG_FINE,
    PC5_LOG_CRASHED,          /* the process ended without closing its log */
    PC5_LOG_GAME_CRASHED,     /* Wine reported an unhandled exception */
    PC5_LOG_NO_PICTURE,       /* the game closed within a minute, having shown nothing */
    PC5_LOG_FORCED,           /* the game didn't close when asked and was ended */
    PC5_LOG_PC5_PROBLEM,      /* PC5 logged a problem (an install or uninstall that failed) */
} Pc5LogProblem;

typedef struct Pc5LogIssue {
    Pc5LogProblem problem;
    char profile[PC5_ID];     /* the game's id, or "launcher" for PC5 */
    char detail[200];         /* Wine's line about the crash, or PC5's problem */
    char file[PC5_PATH];      /* where the log is kept */
    int64_t time;             /* when the session began (seconds since 1970) */
} Pc5LogIssue;

/* What a session's log says: the problem, if any. */
Pc5LogProblem pc5_log_classify(const char *text, size_t length, Pc5LogIssue *issue);

/* Tidies root/logs as above, leaving the session now running alone. now is
 * the time in seconds since 1970 and offset_s the console's offset from UTC,
 * for the kept logs' names. Returns how many sessions had a problem; the
 * newest of them is in *newest (when not NULL). */
int pc5_logs_tidy(const char *root, int keep_all, int64_t now, int offset_s, Pc5LogIssue *newest);

#endif
