/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: the startup sound's WAV file (pc5_sound.h). */
#include "pc5_sound.h"

#include <string.h>

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int pc5_sound_from_wav(const uint8_t *data, size_t size, Pc5Sound *sound)
{
    const uint8_t *samples = NULL;
    size_t bytes = 0;
    int channels = 0, have_format = 0;

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
            uint16_t format = le16(body), bits = le16(body + 14);
            /* WAVE_FORMAT_EXTENSIBLE carries PCM's tag in its sub-format. */
            if (format == 0xfffe && length >= 26) format = le16(body + 24);
            channels = le16(body + 2);
            if (format != 1 || bits != 16 || le32(body + 4) != PC5_SOUND_RATE || (channels != 1 && channels != 2))
                return -1;
            have_format = 1;
        } else if (!memcmp(data + at, "data", 4) && !samples) {
            samples = body;
            bytes = length;
        }
        at += 8 + (size_t)length + (length & 1);
    }
    if (!have_format || !samples) return -1;
    sound->frames = bytes / (size_t)(2 * channels);
    if (!sound->frames || !(sound->samples = pc5_alloc(sound->frames * sizeof(int16_t)))) {
        sound->frames = 0;
        return -1;
    }
    for (size_t i = 0; i < sound->frames; i++) {
        const uint8_t *frame = samples + i * (size_t)(2 * channels);
        int value = (int16_t)le16(frame);
        if (channels == 2) value = (value + (int16_t)le16(frame + 2)) / 2;
        sound->samples[i] = (int16_t)value;
    }
    return 0;
}

void pc5_sound_free(Pc5Sound *sound)
{
    pc5_free(sound->samples);
    sound->samples = NULL;
    sound->frames = 0;
}
