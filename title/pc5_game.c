/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5 in a game's process (pc5_game.h). */
#include "pc5_game.h"
#include "pw_pc5_hud.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { LIGHT_EVERY_MS = 100 };

static void say(Pc5GameSession *game, const char *format, ...) __attribute__((format(printf, 2, 3)));
static void say(Pc5GameSession *game, const char *format, ...)
{
    char line[300];
    va_list args;
    if (!game->ops.log) return;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    game->ops.log(line);
}

/* A colour at a light bar's brightness: its strongest channel to 230. */
static uint32_t bright(uint32_t rgb)
{
    uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255, hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
    if (hi < 8) return 0;
    return (r * 230 / hi) << 16 | (g * 230 / hi) << 8 | (b * 230 / hi);
}

/* The tint in the cover file's header ("PC5D" or "PC5C", width, height, tint). */
static uint32_t cover_tint(const char *root, const char *prefix)
{
    char path[PC5_PATH];
    uint8_t head[16];
    int fd;

    if (!prefix[0] || pc5_format(path, sizeof(path), "%s/prefixes/%s/pc5/cover.bin", root, prefix) ||
        (fd = open(path, O_RDONLY)) < 0)
        return 0;
    int got = (int)read(fd, head, sizeof(head));
    close(fd);
    if (got != 16 || memcmp(head, "PC5", 3)) return 0;
    return ((uint32_t)head[12] | (uint32_t)head[13] << 8 | (uint32_t)head[14] << 16) & 0xffffffu;
}

void pc5_game_begin(Pc5GameSession *game, const Pc5GameOps *ops, const char *root, const char *app,
                    const char *prefix, const char *title)
{
    memset(game, 0, sizeof(*game));
    game->ops = *ops;
    pc5_settings_load(root, &game->settings);
    pc5_copy(game->app, sizeof(game->app), app ? app : "");
    pc5_copy(game->title, sizeof(game->title), title ? title : "");
    game->cover_tint = cover_tint(root, prefix ? prefix : "");
    game->light_target = bright(game->cover_tint);
    game->trigger_amplitude = 0;
    game->resistance = game->settings.triggers == PC5_TRIGGERS_RESISTANCE || game->settings.triggers == PC5_TRIGGERS_BOTH;
    say(game, "PC5 game fps=%d corner=%d vibration=%d light=%d triggers=%d trigger_rumble=%d trigger_resistance=%d "
        "tint=%06x", game->settings.fps, (int)game->settings.fps_corner, game->settings.vibration,
        (int)game->settings.light_bar, (int)game->settings.triggers, game->settings.trigger_rumble,
        game->settings.trigger_resistance, (unsigned)game->cover_tint);
}

const uint32_t *pc5_game_loading(Pc5GameSession *game, uint64_t now_ms)
{
    if (game->matrix_failed) return NULL;
    if (!game->matrix) {
        Pc5Fonts fonts;
        char path[PC5_PATH];
        memset(&fonts, 0, sizeof(fonts));
        if (pc5_format(path, sizeof(path), "%s/pc5/fonts.bin", game->app) || pc5_fonts_load(&fonts, path))
            say(game, "PC5 loading screen without its fonts: %s", path);
        game->matrix = pc5_matrix_create(&fonts, game->title, (uint32_t)now_ms * 2654435761u);
        pc5_fonts_free(&fonts);
        if (!game->matrix) {
            game->matrix_failed = 1;
            say(game, "PC5 loading screen: out of memory");
            return NULL;
        }
        game->matrix_at = 0;
    }
    if (game->matrix_at && now_ms < game->matrix_at) return NULL;
    game->matrix_at = now_ms + PC5_MATRIX_STEP_MS;
    return pc5_matrix_step(game->matrix);
}

void pc5_game_loading_done(Pc5GameSession *game)
{
    pc5_matrix_destroy(game->matrix);
    game->matrix = NULL;
    game->matrix_failed = 1;   /* not again */
}

/* A strength in percent as the triggers' 1-8: 25 % is 2, 100 % is 8. */
static uint8_t trigger_level(int percent)
{
    int level = percent * 8 / 100;
    return (uint8_t)(level < 1 ? 1 : level > 8 ? 8 : level);
}

static void set_trigger_pair(Pc5GameSession *game, int pad, const Pc5Trigger *l2, const Pc5Trigger *r2,
                             const char *why)
{
    if (!game->ops.triggers || pad < 0) return;
    int rc = game->ops.triggers(pad, l2, r2);
    if (game->triggers_sent++ && !rc) return;
    if (l2->mode == PC5_TRIGGER_GAME || r2->mode == PC5_TRIGGER_GAME)
        say(game, "PC5 triggers %s effects=%d,%d rc=%d", why, l2->effect, r2->effect, rc);
    else
        say(game, "PC5 triggers %s mode=%d rc=%d", why, (int)l2->mode, rc);
}

static void set_triggers(Pc5GameSession *game, int pad, const Pc5Trigger *both, const char *why)
{
    set_trigger_pair(game, pad, both, both, why);
}

void pc5_game_rumble(Pc5GameSession *game, int pad, uint32_t left, uint32_t right, uint8_t *large, uint8_t *small)
{
    /* The motors at Vibration's strength (0 % is off). */
    *large = (uint8_t)((left >> 8) * (uint32_t)game->settings.vibration / 100);
    *small = (uint8_t)((right >> 8) * (uint32_t)game->settings.vibration / 100);
    if (!game->game_triggers &&
        (game->settings.triggers == PC5_TRIGGERS_RUMBLE || game->settings.triggers == PC5_TRIGGERS_BOTH)) {
        /* The stronger motor's speed as the triggers' amplitude: 1 up to 2, 4, 6
         * or 8 at the triggers' rumble strength (25 to 100 %), 0 when the game
         * stops. Then they are off again, or push back (both). */
        uint32_t speed = left > right ? left : right, most = (uint32_t)trigger_level(game->settings.trigger_rumble);
        uint8_t amplitude = (uint8_t)(speed ? 1 + (speed * (most - 1) + 32767) / 65535 : 0);
        if (amplitude != game->trigger_amplitude) {
            game->trigger_amplitude = amplitude;
            if (amplitude) {
                Pc5Trigger both = { .mode = PC5_TRIGGER_VIBRATION, .position = 2, .amplitude = amplitude,
                                     .frequency = (uint8_t)(left >= right ? 30 : 60) };
                game->resistance = 0;   /* not over the rumble */
                set_triggers(game, pad, &both, "rumble");
            } else if (game->settings.triggers == PC5_TRIGGERS_BOTH) {
                game->resistance = 1;   /* pc5_game_pad sends it */
            } else {
                Pc5Trigger off = { .mode = PC5_TRIGGER_OFF };
                set_triggers(game, pad, &off, "rumble");
            }
        }
    }
}

static int same_trigger(const Pc5Trigger *a, const Pc5Trigger *b)
{
    return a->mode == b->mode && a->effect == b->effect && !memcmp(a->data, b->data, sizeof(a->data));
}

void pc5_game_output(Pc5GameSession *game, int pad, const Pc5PadOutput *output, uint8_t *large, uint8_t *small)
{
    uint32_t light = output->light & 0xffffffu;

    if ((output->set & PC5_OUTPUT_LIGHT) && game->ops.light_bar && pad >= 0 &&
        (!game->game_light || light != game->light)) {
        int rc = game->ops.light_bar(pad, light);
        if (!game->game_light || rc) say(game, "PC5 light bar the game's %06x rc=%d", (unsigned)light, rc);
        game->game_light = 1;
        game->light = light;
        game->light_sent = 1;
    }
    if ((output->set & PC5_OUTPUT_TRIGGERS) &&
        (!game->game_triggers || !same_trigger(&output->trigger[0], &game->game_trigger[0]) ||
         !same_trigger(&output->trigger[1], &game->game_trigger[1]))) {
        game->game_triggers = 1;
        game->resistance = 0;
        game->trigger_amplitude = 0;
        game->game_trigger[0] = output->trigger[0];
        game->game_trigger[1] = output->trigger[1];
        set_trigger_pair(game, pad, &output->trigger[0], &output->trigger[1], "game");
    }
    pc5_game_rumble(game, pad, (uint32_t)output->large * 257u, (uint32_t)output->small * 257u, large, small);
}

void pc5_game_pad(Pc5GameSession *game, int pad, uint64_t now_ms, uint32_t screen)
{
    if (pad < 0) return;
    if (game->resistance && !game->game_triggers) {
        /* Pushing back from a third of the way in, 2 (25 %) to 8 (100 %) firm. */
        Pc5Trigger both = { .mode = PC5_TRIGGER_FEEDBACK, .position = 3,
                             .strength = trigger_level(game->settings.trigger_resistance) };
        game->resistance = 0;
        set_triggers(game, pad, &both, "resistance");
    }
    if (game->settings.light_bar == PC5_LIGHT_CONSOLE || !game->ops.light_bar || game->game_light) return;
    if (game->light_at && now_ms - game->light_at < LIGHT_EVERY_MS) return;
    game->light_at = now_ms;
    if (game->settings.light_bar == PC5_LIGHT_SCREEN && screen) game->light_target = bright(screen);
    else if (game->settings.light_bar == PC5_LIGHT_GAME) game->light_target = bright(game->cover_tint);
    if (!game->light_target && !game->light_sent) return;   /* nothing to show yet: the console's own */
    /* A quarter of the way each step: changes of picture fade in. */
    uint32_t next = 0;
    for (int shift = 0; shift < 24; shift += 8) {
        int now = (int)((game->light >> shift) & 255), target = (int)((game->light_target >> shift) & 255);
        int step = game->light_sent ? (target - now) / 4 : target - now;
        if (!step && now != target) step = target > now ? 1 : -1;
        next |= (uint32_t)(now + step) << shift;
    }
    if (game->light_sent && next == game->light) return;
    int rc = game->ops.light_bar(pad, next);
    if (!game->light_sent || rc) say(game, "PC5 light bar %06x rc=%d", (unsigned)next, rc);
    game->light = next;
    game->light_sent = 1;
}

void pc5_game_hud(Pc5GameSession *game, int videoout, uint64_t frames, uint64_t now_ms)
{
    uint32_t image[PW_PC5_HUD_SIDE * PW_PC5_HUD_SIDE], x, y;

    if (!game->settings.fps || !game->ops.cursor || videoout < 0) return;
    if (!game->hud_second_at) {
        game->hud_second_at = now_ms;
        game->hud_second_frames = frames;
        return;
    }
    if (now_ms - game->hud_second_at < 1000) return;
    unsigned fps = (unsigned)(((frames - game->hud_second_frames) * 1000 + (now_ms - game->hud_second_at) / 2) /
                              (now_ms - game->hud_second_at));
    game->hud_second_at = now_ms;
    game->hud_second_frames = frames;
    pw_pc5_hud_draw(image, fps);
    pw_pc5_hud_place((int)game->settings.fps_corner, 1920, 1080, &x, &y);
    if (game->ops.cursor(videoout, image, x, y) && !game->hud_shown) say(game, "PC5 fps counter refused");
    game->hud_shown = 1;
}

void pc5_game_end(Pc5GameSession *game, int pad)
{
    Pc5Trigger off = { .mode = PC5_TRIGGER_OFF };
    if (pad >= 0 && game->triggers_sent && game->ops.triggers) game->ops.triggers(pad, &off, &off);
    if (pad >= 0 && game->light_sent && game->ops.light_bar_reset) game->ops.light_bar_reset(pad);
    pc5_matrix_destroy(game->matrix);
    game->matrix = NULL;
}
