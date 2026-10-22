/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: what shows while a game starts, until its first picture: green code
 * falling down the screen, mirrored letters and digits leaving fading
 * trails, with the game's name in the middle. Drawn on the CPU into a
 * 1280x720 B,G,R,X frame (the title shows it scaled to the screen), a step
 * every 50 ms, each step only darkening the frame and drawing the heads. */
#ifndef PC5_MATRIX_H
#define PC5_MATRIX_H
#include "pc5.h"
#include "pc5_draw.h"

enum { PC5_MATRIX_WIDTH = 1280, PC5_MATRIX_HEIGHT = 720, PC5_MATRIX_STEP_MS = 50 };

typedef struct Pc5Matrix Pc5Matrix;

/* The fonts are taken over (and freed with the matrix). NULL when out of memory. */
Pc5Matrix *pc5_matrix_create(Pc5Fonts *fonts, const char *title, uint32_t seed);
void pc5_matrix_destroy(Pc5Matrix *matrix);
/* Draws the next step; the frame stays valid until the next call. */
const uint32_t *pc5_matrix_step(Pc5Matrix *matrix);

#endif
