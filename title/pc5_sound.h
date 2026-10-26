/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: the startup sound (the app's pc5/startup.wav), read for the
 * controller's speaker, which plays 16-bit mono at 48 kHz. */
#ifndef PC5_SOUND_H
#define PC5_SOUND_H
#include "pc5.h"

enum { PC5_SOUND_RATE = 48000 };

typedef struct Pc5Sound {
    int16_t *samples;        /* mono */
    size_t frames;
} Pc5Sound;

/* A WAV file's samples: 16-bit PCM at 48 kHz, mono or stereo (mixed to one).
 * 0, or -1 for anything else. */
int pc5_sound_from_wav(const uint8_t *data, size_t size, Pc5Sound *sound);
void pc5_sound_free(Pc5Sound *sound);

#endif
