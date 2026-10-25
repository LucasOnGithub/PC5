/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: writing a .zip, one file after another, for the logs a player attaches
 * to a report (pc5_logs.c). Each file is deflated (stb_image_write's
 * compressor, the one the home screen tiles' PNGs use) unless that doesn't
 * make it smaller, when it is stored. The archive is written to path.part
 * and renamed into place once complete, so a half-written one never shows. */
#ifndef PC5_ZIP_H
#define PC5_ZIP_H
#include "pc5.h"

typedef struct Pc5Zip {
    int fd;
    int failed;
    uint32_t count;
    uint64_t offset;
    Pc5Buf directory;
    char path[PC5_PATH];
} Pc5Zip;

int pc5_zip_open(Pc5Zip *zip, const char *path);
/* name inside the archive ('/' between folders); time is the file's time on
 * the console's clock (seconds since 1970, local), for the date the archive
 * shows. */
int pc5_zip_add(Pc5Zip *zip, const char *name, const void *data, size_t size, int64_t time);
/* Writes the archive's directory and puts it in place: 0, or -1 when any
 * step failed (nothing is left behind then). */
int pc5_zip_close(Pc5Zip *zip);
/* Leaves without writing anything. */
void pc5_zip_abandon(Pc5Zip *zip);

uint32_t pc5_crc32(const void *data, size_t size);

#endif
