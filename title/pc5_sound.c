/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: the startup sounds (pc5_sound.h). */
#include "pc5_sound.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* One sample of a frame's channel, as 16 bits. */
static int sample(const uint8_t *p, int bits, int is_float)
{
    if (is_float) {
        uint32_t u = le32(p);
        float f;
        memcpy(&f, &u, sizeof(f));
        if (!(f == f)) return 0;   /* NaN */
        f = f > 1 ? 1 : f < -1 ? -1 : f;
        return (int)(f * 32767);
    }
    if (bits == 8) return ((int)p[0] - 128) * 256;
    if (bits == 16) return (int16_t)le16(p);
    return (int)((int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 16);   /* 24 */
}

int pc5_sound_from_wav(const uint8_t *data, size_t size, Pc5Sound *sound)
{
    const uint8_t *samples = NULL;
    size_t bytes = 0;
    int channels = 0, bits = 0, is_float = 0, have_format = 0;
    uint32_t rate = 0;

    memset(sound, 0, sizeof(*sound));
    if (size < 12 || memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4)) return -1;
    /* Chunks: "fmt " says what the samples are, "data" holds them. */
    for (size_t at = 12; at + 8 <= size;) {
        uint32_t length = le32(data + at + 4);
        const uint8_t *body = data + at + 8;
        if (length > size - at - 8) {
            if (memcmp(data + at, "data", 4)) return -1;
            length = (uint32_t)(size - at - 8);   /* a file cut short: what is there */
        }
        if (!memcmp(data + at, "fmt ", 4)) {
            if (length < 16) return -1;
            uint16_t format = le16(body);
            /* WAVE_FORMAT_EXTENSIBLE carries the format's tag in its sub-format. */
            if (format == 0xfffe && length >= 26) format = le16(body + 24);
            channels = le16(body + 2);
            rate = le32(body + 4);
            bits = le16(body + 14);
            is_float = format == 3;
            if ((format != 1 && format != 3) || (is_float && bits != 32) ||
                (!is_float && bits != 8 && bits != 16 && bits != 24) || (channels != 1 && channels != 2) ||
                rate < 8000 || rate > 192000)
                return -1;
            have_format = 1;
        } else if (!memcmp(data + at, "data", 4) && !samples) {
            samples = body;
            bytes = length;
        }
        at += 8 + (size_t)length + (length & 1);
    }
    if (!have_format || !samples) return -1;
    size_t width = (size_t)(bits / 8), frame_bytes = width * (size_t)channels, in = bytes / frame_bytes;
    if (in > (size_t)rate * PC5_SOUND_SECONDS) in = (size_t)rate * PC5_SOUND_SECONDS;
    sound->frames = (size_t)((uint64_t)in * PC5_SOUND_RATE / rate);
    if (!in || !sound->frames || !(sound->samples = pc5_alloc(sound->frames * sizeof(int16_t)))) {
        sound->frames = 0;
        return -1;
    }
    /* Mixed to one channel, then to 48 kHz: each new sample between the two old ones around it. */
    for (size_t i = 0; i < sound->frames; i++) {
        uint64_t at = (uint64_t)i * rate * 256 / PC5_SOUND_RATE;   /* in 1/256ths of an old frame */
        size_t a = (size_t)(at >> 8), b = a + 1 < in ? a + 1 : a;
        int t = (int)(at & 255), va = 0, vb = 0;
        for (int c = 0; c < channels; c++) {
            va += sample(samples + a * frame_bytes + (size_t)c * width, bits, is_float);
            vb += sample(samples + b * frame_bytes + (size_t)c * width, bits, is_float);
        }
        int value = (va * (256 - t) + vb * t) / (256 * channels);
        sound->samples[i] = (int16_t)(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
    }
    return 0;
}

void pc5_sound_free(Pc5Sound *sound)
{
    pc5_free(sound->samples);
    sound->samples = NULL;
    sound->frames = 0;
}

static int name_order(const void *a, const void *b)
{
    return pc5_casecmp((const char *)a, (const char *)b);
}

/* The .wav files in dir, by name, added to list when not there yet. */
static void list_in(const char *dir, Pc5SoundList *list)
{
    DIR *folder = opendir(dir);
    struct dirent *entry;

    if (!folder) return;
    while ((entry = readdir(folder)) && list->count < PC5_SOUNDS) {
        size_t n = strlen(entry->d_name);
        if (entry->d_name[0] == '.' || n <= 4 || n - 4 >= PC5_SOUND_NAME || !pc5_ends_with_ci(entry->d_name, ".wav") ||
            entry->d_name[0] == ' ' || entry->d_name[0] == '\t' || entry->d_name[n - 5] == ' ' || entry->d_name[n - 5] == '\t')
            continue;   /* a name the settings file couldn't keep as it is */
        char name[PC5_SOUND_NAME];
        memcpy(name, entry->d_name, n - 4);
        name[n - 4] = 0;
        int seen = 0;
        for (int i = 0; i < list->count && !seen; i++) seen = !pc5_casecmp(list->name[i], name);
        if (!seen) pc5_copy(list->name[list->count++], PC5_SOUND_NAME, name);
    }
    closedir(folder);
}

void pc5_sound_list(const char *app, const char *root, Pc5SoundList *list)
{
    char dir[PC5_PATH];

    list->count = 0;
    if (app && !pc5_format(dir, sizeof(dir), "%s/pc5/sounds", app)) list_in(dir, list);
    if (root && !pc5_format(dir, sizeof(dir), "%s/sounds", root)) list_in(dir, list);
    qsort(list->name, (size_t)list->count, sizeof(list->name[0]), name_order);
}

int pc5_sound_path(const char *app, const char *root, const char *name, char *out, size_t size)
{
    char dir[PC5_PATH];
    const char *const names[] = { name, NULL };
    static const char *const wav[] = { ".wav", NULL };
    DIR *folder;
    struct dirent *entry;

    if (!name || !name[0] || strchr(name, '/')) return -1;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0 ? !root || pc5_format(dir, sizeof(dir), "%s/sounds", root)
                      : !app || pc5_format(dir, sizeof(dir), "%s/pc5/sounds", app))
            continue;
        if (!(folder = opendir(dir))) continue;
        /* the file's name in any case, as the list shows it */
        while ((entry = readdir(folder))) {
            size_t n = strlen(entry->d_name);
            if (n <= 4 || !pc5_ends_with_ci(entry->d_name, wav[0]) || n - 4 != strlen(names[0]) ||
                pc5_casecmp(entry->d_name + n - 4, ".wav"))
                continue;
            char base[PC5_SOUND_NAME + 1];
            if (n - 4 >= sizeof(base)) continue;
            memcpy(base, entry->d_name, n - 4);
            base[n - 4] = 0;
            if (!pc5_casecmp(base, name) && !pc5_join(out, size, dir, entry->d_name) && pc5_is_file(out)) {
                closedir(folder);
                return 0;
            }
        }
        closedir(folder);
    }
    return -1;
}
