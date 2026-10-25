/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "pc5_zip.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* stb_image_write's zlib stream (pc5_stb.c): two header bytes, raw deflate,
 * four bytes of Adler-32. A zip holds the raw deflate between them. */
unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);

uint32_t pc5_crc32(const void *data, size_t size)
{
    static uint32_t table[256];
    static int ready;
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;

    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = 1;
    }
    for (size_t i = 0; i < size; i++) crc = table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

static void le16(uint8_t *at, uint32_t value) { at[0] = (uint8_t)value; at[1] = (uint8_t)(value >> 8); }
static void le32(uint8_t *at, uint32_t value)
{
    for (int i = 0; i < 4; i++) at[i] = (uint8_t)(value >> (8 * i));
}

/* The archive's date and time fields (MS-DOS: 2-second steps, from 1980). */
static void dos_time(int64_t time, uint16_t *dos_date, uint16_t *dos_clock)
{
    int64_t days = time / 86400, secs = time % 86400;
    if (secs < 0) { secs += 86400; days--; }
    /* Howard Hinnant's civil_from_days. */
    int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
    if (y < 1980) { *dos_date = (1 << 5) | 1; *dos_clock = 0; return; }
    if (y > 2107) y = 2107;
    *dos_date = (uint16_t)(((y - 1980) << 9) | (m << 5) | d);
    *dos_clock = (uint16_t)(((secs / 3600) << 11) | (((secs / 60) % 60) << 5) | ((secs % 60) / 2));
}

static int put(Pc5Zip *zip, const void *data, size_t size)
{
    const uint8_t *at = data;
    while (size && !zip->failed) {
        ssize_t n = write(zip->fd, at, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { zip->failed = 1; break; }
        at += n;
        size -= (size_t)n;
        zip->offset += (uint64_t)n;
    }
    return zip->failed ? -1 : 0;
}

int pc5_zip_open(Pc5Zip *zip, const char *path)
{
    char part[PC5_PATH + 8];
    memset(zip, 0, sizeof(*zip));
    zip->fd = -1;
    if (pc5_format(zip->path, sizeof(zip->path), "%s", path) || pc5_format(part, sizeof(part), "%s.part", path)) return -1;
    zip->fd = open(part, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    return zip->fd < 0 ? -1 : 0;
}

int pc5_zip_add(Pc5Zip *zip, const char *name, const void *data, size_t size, int64_t time)
{
    size_t name_len = strlen(name);
    uint8_t *packed = NULL;
    const uint8_t *body = data;
    size_t body_len = size;
    uint16_t method = 0, date, clock, flags = 0;
    uint8_t local[30], central[46];

    if (zip->fd < 0 || zip->failed) return -1;
    if (!name_len || name_len > 0xffff || size > 0xfffffff0u || zip->offset > 0xfffffff0u || zip->count >= 0xffff) {
        zip->failed = 1;
        return -1;
    }
    for (size_t i = 0; i < name_len; i++)
        if ((uint8_t)name[i] >= 0x80) flags |= 1u << 11;   /* the name is UTF-8 */
    uint32_t crc = pc5_crc32(data, size);
    if (size >= 64 && size < 0x40000000u) {
        int out = 0;
        packed = stbi_zlib_compress((unsigned char *)(uintptr_t)data, (int)size, &out, 5);
        if (packed && out > 6 && (size_t)(out - 6) < size) {
            body = packed + 2;
            body_len = (size_t)out - 6;
            method = 8;
        }
    }
    dos_time(time, &date, &clock);

    memset(local, 0, sizeof(local));
    le32(local, 0x04034b50u);
    le16(local + 4, 20);
    le16(local + 6, flags);
    le16(local + 8, method);
    le16(local + 10, clock);
    le16(local + 12, date);
    le32(local + 14, crc);
    le32(local + 18, (uint32_t)body_len);
    le32(local + 22, (uint32_t)size);
    le16(local + 26, (uint32_t)name_len);

    memset(central, 0, sizeof(central));
    le32(central, 0x02014b50u);
    le16(central + 4, 0x0314);         /* made by: Unix, zip 2.0 */
    le16(central + 6, 20);
    le16(central + 8, flags);
    le16(central + 10, method);
    le16(central + 12, clock);
    le16(central + 14, date);
    le32(central + 16, crc);
    le32(central + 20, (uint32_t)body_len);
    le32(central + 24, (uint32_t)size);
    le16(central + 28, (uint32_t)name_len);
    le32(central + 38, 0100644u << 16);   /* a plain file, rw-r--r-- */
    le32(central + 42, (uint32_t)zip->offset);

    if (pc5_buf_put(&zip->directory, central, sizeof(central)) || pc5_buf_put(&zip->directory, name, name_len))
        zip->failed = 1;
    put(zip, local, sizeof(local));
    put(zip, name, name_len);
    put(zip, body, body_len);
    if (packed) pc5_free(packed);
    if (!zip->failed) zip->count++;
    return zip->failed ? -1 : 0;
}

void pc5_zip_abandon(Pc5Zip *zip)
{
    char part[PC5_PATH + 8];
    if (zip->fd >= 0) close(zip->fd);
    zip->fd = -1;
    if (!pc5_format(part, sizeof(part), "%s.part", zip->path)) unlink(part);
    pc5_buf_free(&zip->directory);
}

int pc5_zip_close(Pc5Zip *zip)
{
    char part[PC5_PATH + 8];
    uint8_t end[22];
    uint64_t at = zip->offset;

    if (zip->fd < 0) return -1;
    memset(end, 0, sizeof(end));
    le32(end, 0x06054b50u);
    le16(end + 8, zip->count);
    le16(end + 10, zip->count);
    le32(end + 12, (uint32_t)zip->directory.len);
    le32(end + 16, (uint32_t)at);
    if (zip->directory.len) put(zip, zip->directory.data, zip->directory.len);
    put(zip, end, sizeof(end));
    if (!zip->failed && fsync(zip->fd)) zip->failed = 1;
    if (close(zip->fd)) zip->failed = 1;
    zip->fd = -1;
    pc5_buf_free(&zip->directory);
    if (pc5_format(part, sizeof(part), "%s.part", zip->path)) return -1;
    if (zip->failed || rename(part, zip->path)) {
        unlink(part);
        return -1;
    }
    return 0;
}
