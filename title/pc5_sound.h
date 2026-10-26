/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: the startup sounds, played on the controller's speaker (16-bit mono at
 * 48 kHz) when PC5 is opened. The app brings its own (pc5/sounds/<name>.wav);
 * the player can add more as root/sounds/<name>.wav. Settings › In games ›
 * Startup sound chooses one by name, or Off. */
#ifndef PC5_SOUND_H
#define PC5_SOUND_H
#include "pc5.h"

enum {
    PC5_SOUND_RATE = 48000,
    PC5_SOUND_SECONDS = 30,      /* longer sounds are cut */
    PC5_SOUNDS = 32,             /* names in a list */
    PC5_SOUND_NAME = 64,
};
#define PC5_SOUND_DEFAULT "PC5"

typedef struct Pc5Sound {
    int16_t *samples;        /* mono, at PC5_SOUND_RATE */
    size_t frames;
} Pc5Sound;

/* A WAV file's samples: PCM of 8, 16 or 24 bits or 32-bit float, mono or stereo (mixed
 * to one), at any rate (made 48 kHz). 0, or -1 for anything else. */
int pc5_sound_from_wav(const uint8_t *data, size_t size, Pc5Sound *sound);
void pc5_sound_free(Pc5Sound *sound);

typedef struct Pc5SoundList {
    char name[PC5_SOUNDS][PC5_SOUND_NAME];
    int count;
} Pc5SoundList;

/* The sounds to choose from, by name (the file's, without .wav): the app's in app/pc5/sounds,
 * then the player's in root/sounds; sorted, each name once. */
void pc5_sound_list(const char *app, const char *root, Pc5SoundList *list);
/* A sound's file: the player's of that name, else the app's. 0, or -1 when there is none. */
int pc5_sound_path(const char *app, const char *root, const char *name, char *out, size_t size);

#endif
