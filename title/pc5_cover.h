/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: finding a game's cover online, from public sources only, with no
 * account and no key: Steam's store, GOG's catalogue and Wikipedia, in that
 * order. Each lookup is one request, so the screen can move between them. */
#ifndef PC5_COVER_H
#define PC5_COVER_H
#include "pc5.h"

/* How PC5 asks the network for something (the console's HTTPS is
 * pc5_net.c over pc5_net_ps5.c; the tests answer from files). */
typedef struct Pc5Http {
    void *context;
    /* GET url into out (at most max bytes). 0 with the HTTP status in
     * *status, or -1 when no answer came at all. */
    int (*get)(void *context, const char *url, Pc5Buf *out, size_t max, int *status);
} Pc5Http;

/* What PC5 knows about the game before asking. */
typedef struct Pc5CoverHint {
    char folder[PC5_NAME];      /* the folder's name, as on the USB drive */
    char product[PC5_NAME];     /* the program's ProductName, or "" */
    char program[PC5_NAME];     /* the .exe's name without .exe */
    uint32_t steam_appid;       /* from a steam_appid.txt in the folder, or 0 */
} Pc5CoverHint;

enum { PC5_COVER_CHOICES = 12 };

typedef struct Pc5CoverChoice {
    char source[16];            /* "Steam", "GOG", "Wikipedia" */
    char title[128];            /* the game's name there */
    char url[512];              /* its cover picture */
    uint32_t steam_appid;       /* Steam: the app, for its asset list */
    int score;
} Pc5CoverChoice;

typedef struct Pc5CoverSearch {
    Pc5CoverChoice choices[PC5_COVER_CHOICES];
    int count;
    /* where the search is */
    char queries[3][PC5_NAME];
    int query_count, step, requests, answered;
    Pc5CoverHint hint;
} Pc5CoverSearch;

/* The words to search for, best first: the product name, the folder's name
 * cleaned of release tags and versions, the program's name. */
int pc5_cover_queries(const Pc5CoverHint *hint, char queries[3][PC5_NAME]);
/* How alike two game names are, 0 to 100 (word overlap, "GTA" matching
 * "Grand Theft Auto", edition words ignored). */
int pc5_cover_similarity(const char *a, const char *b);

void pc5_cover_search_start(Pc5CoverSearch *search, const Pc5CoverHint *hint);
/* One request. 1 while there is more to ask, 0 when done (choices best
 * first). A source that does not answer is skipped. */
int pc5_cover_search_step(Pc5CoverSearch *search, const Pc5Http *http);
/* A choice's picture, fetched and made a cover (pc5_cover_compose). original,
 * when not NULL, gets the picture's file as it was downloaded (JPEG or PNG). */
int pc5_cover_fetch(const Pc5CoverChoice *choice, const Pc5Http *http, Pc5Image *cover, Pc5Buf *original);
/* Where a game's downloaded picture is kept, by its kind: "cover-original.jpg" or ".png". */
const char *pc5_picture_name(const Pc5Buf *original);

/* text with everything but letters, digits and -._~ as %XX. */
int pc5_url_encode(const char *text, char *out, size_t size);

#endif
