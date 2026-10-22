/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5 in a game's process (prospero-win's title in game mode, PW_PC5): its
 * settings applied while the game runs. The controller: the game's rumble
 * at the strength chosen, the light bar in the game's colour or the
 * screen's, the triggers' rumble, resistance or both. The loading screen until the game's first
 * picture (pc5_matrix.h). The frame counter over the frames the title shows
 * itself (a game drawing with Vulkan gets libvulkan's, pw_pc5_hud). The
 * console's calls come through Pc5GameOps (pc5_game_ps5.c), so the host
 * tests can watch them. */
#ifndef PC5_GAME_H
#define PC5_GAME_H
#include "pc5.h"
#include "pc5_matrix.h"
#include "pc5_settings.h"

typedef enum Pc5TriggerMode { PC5_TRIGGER_OFF, PC5_TRIGGER_FEEDBACK, PC5_TRIGGER_VIBRATION } Pc5TriggerMode;

typedef struct Pc5Trigger {
    Pc5TriggerMode mode;
    uint8_t position;     /* where along the travel it starts, 0-9 */
    uint8_t strength;     /* feedback: 1-8 */
    uint8_t amplitude;    /* vibration: 1-8 */
    uint8_t frequency;    /* vibration: Hz */
} Pc5Trigger;

typedef struct Pc5GameOps {
    int (*light_bar)(int handle, uint32_t rgb);         /* 0xRRGGBB */
    int (*light_bar_reset)(int handle);
    int (*triggers)(int handle, const Pc5Trigger *l2, const Pc5Trigger *r2);
    /* The second hardware cursor of the title's own VideoOut: a 64x64 image at
     * (x, y), or hidden (image NULL). */
    int (*cursor)(int videoout, const uint32_t *image, uint32_t x, uint32_t y);
    void (*log)(const char *line);
} Pc5GameOps;

typedef struct Pc5GameSession {
    Pc5GameOps ops;
    Pc5Settings settings;
    char app[PC5_PATH], title[PC5_NAME];
    uint32_t cover_tint;          /* the game's colour, 0 when it has no cover */
    uint32_t light, light_target; /* what the light bar shows and is going to */
    uint64_t light_at;
    int light_sent, triggers_sent, resistance;
    uint8_t trigger_amplitude;
    Pc5Matrix *matrix;
    uint64_t matrix_at;
    int matrix_failed;
    int hud_shown;
    uint64_t hud_second_at, hud_second_frames;
} Pc5GameSession;

/* At the game's start: PC5's settings and the game's cover colour, from
 * root (/data/prospero-win), the game's prefix and the app's folder. */
void pc5_game_begin(Pc5GameSession *game, const Pc5GameOps *ops, const char *root, const char *app,
                    const char *prefix, const char *title);
/* The loading screen's next frame (PC5_MATRIX_WIDTH x HEIGHT, B,G,R,X), or
 * NULL when none is due yet (or it can't be drawn). */
const uint32_t *pc5_game_loading(Pc5GameSession *game, uint64_t now_ms);
/* The game showed its first picture. */
void pc5_game_loading_done(Pc5GameSession *game);
/* The game's rumble (XInput's 0-65535 a motor): what the motors get, at
 * Vibration's strength, and the triggers when they follow it. */
void pc5_game_rumble(Pc5GameSession *game, int pad, uint32_t left, uint32_t right, uint8_t *large, uint8_t *small);
/* Every tick of the title's loop: the light bar (screen: the colour of the
 * frame on screen, 0 when unknown), the triggers' resistance. */
void pc5_game_pad(Pc5GameSession *game, int pad, uint64_t now_ms, uint32_t screen);
/* Every tick while the title shows the frames itself: frames is how many it
 * has shown. */
void pc5_game_hud(Pc5GameSession *game, int videoout, uint64_t frames, uint64_t now_ms);
/* The game has ended: the controller as the console had it. */
void pc5_game_end(Pc5GameSession *game, int pad);

#endif
