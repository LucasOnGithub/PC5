/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5's game ops on the console (pc5_game.h): libScePad's light bar and
 * adaptive triggers, and VideoOut's second hardware cursor for the frame
 * counter over the frames the title shows itself. */
#include "pc5_game.h"

#include <stdint.h>
#include <string.h>
#include <x86intrin.h>

typedef struct LightBar { uint8_t r, g, b, reserved; } LightBar;
int scePadSetLightBar(int32_t handle, const LightBar *light);
int scePadResetLightBar(int32_t handle);

/* The adaptive triggers (libScePad's ScePadTriggerEffectParam): which
 * triggers, then a command for L2 and one for R2, each a mode and its
 * numbers. Feedback: where it starts (0-9) and how firm (1-8); vibration:
 * where it starts, how strong (1-8) and how fast (Hz); a game's own effect
 * (DualSense mode) comes with its mode, 1 to 6, and its numbers. */
enum { TRIGGER_L2 = 0x01, TRIGGER_R2 = 0x02, MODE_OFF = 0, MODE_FEEDBACK = 1, MODE_VIBRATION = 3,
       MODE_MULTIPLE_VIBRATION = 6 };
typedef struct TriggerCommand { uint32_t mode; uint8_t padding[4]; uint8_t data[48]; } TriggerCommand;
typedef struct TriggerParam { uint8_t mask; uint8_t padding[7]; TriggerCommand command[2]; } TriggerParam;
_Static_assert(sizeof(TriggerParam) == 120, "ScePadTriggerEffectParam is 120 bytes");
int scePadSetTriggerEffect(int32_t handle, const TriggerParam *param);

int sceVideoOutCursorEnable(int32_t handle, int32_t index, const void *address);
int sceVideoOutCursorDisable(int32_t handle, int32_t index);
int sceVideoOutCursorSetImageAddress(int32_t handle, int32_t index, const void *address);
int sceVideoOutCursorSetPosition(int32_t handle, int32_t index, uint32_t x, uint32_t y);
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAllocateDirectMemory(int64_t start, int64_t end, size_t bytes, size_t alignment, int type, int64_t *offset);
int sceKernelMapDirectMemory(void **address, size_t bytes, int protection, int flags, int64_t offset, size_t alignment);

static int light_bar(int handle, uint32_t rgb)
{
    const LightBar light = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb, 0 };
    return scePadSetLightBar(handle, &light);
}

static int light_bar_reset(int handle)
{
    return scePadResetLightBar(handle);
}

static void command(TriggerCommand *out, const Pc5Trigger *trigger)
{
    memset(out, 0, sizeof(*out));
    switch (trigger->mode) {
    case PC5_TRIGGER_FEEDBACK:
        out->mode = MODE_FEEDBACK;
        out->data[0] = trigger->position;
        out->data[1] = trigger->strength;
        break;
    case PC5_TRIGGER_VIBRATION:
        out->mode = MODE_VIBRATION;
        out->data[0] = trigger->position;
        out->data[1] = trigger->amplitude;
        out->data[2] = trigger->frequency;
        break;
    case PC5_TRIGGER_GAME:
        /* the game's effect, already in libScePad's terms */
        out->mode = trigger->effect <= MODE_MULTIPLE_VIBRATION ? trigger->effect : MODE_OFF;
        if (out->mode != MODE_OFF) memcpy(out->data, trigger->data, sizeof(trigger->data));
        break;
    default:
        out->mode = MODE_OFF;
        break;
    }
}

static int triggers(int handle, const Pc5Trigger *l2, const Pc5Trigger *r2)
{
    TriggerParam param;
    memset(&param, 0, sizeof(param));
    param.mask = TRIGGER_L2 | TRIGGER_R2;
    command(&param.command[0], l2);
    command(&param.command[1], r2);
    return scePadSetTriggerEffect(handle, &param);
}

/* Two 64x64 images in display-readable memory, written in turn. */
static uint32_t *cursor_images;
static int cursor_failed, cursor_enabled, cursor_current;

static int cursor(int videoout, const uint32_t *image, uint32_t x, uint32_t y)
{
    enum { PIXELS = 64 * 64, MEMORY = 64 << 10 };
    if (!image) {
        if (cursor_enabled) sceVideoOutCursorDisable(videoout, 1);
        cursor_enabled = 0;
        return 0;
    }
    if (!cursor_images) {
        int64_t offset;
        void *address = NULL;
        if (cursor_failed) return -1;
        if (sceKernelAllocateDirectMemory(0, (int64_t)sceKernelGetDirectMemorySize(), MEMORY, MEMORY, 3, &offset) < 0 ||
            sceKernelMapDirectMemory(&address, MEMORY, 0x33, 0, offset, MEMORY) < 0) {
            cursor_failed = 1;
            return -1;
        }
        cursor_images = address;
    }
    uint32_t *next = cursor_images + (cursor_enabled ? cursor_current ^ 1 : cursor_current) * PIXELS;
    memcpy(next, image, PIXELS * 4);
    for (size_t at = 0; at < PIXELS * 4; at += 64) _mm_clflush((const uint8_t *)next + at);
    _mm_mfence();
    int rc = cursor_enabled ? sceVideoOutCursorSetImageAddress(videoout, 1, next) : sceVideoOutCursorEnable(videoout, 1, next);
    if (rc) return rc;
    cursor_current = (int)((next - cursor_images) / PIXELS);
    cursor_enabled = 1;
    return sceVideoOutCursorSetPosition(videoout, 1, x, y);
}

void pc5_game_ps5_ops(Pc5GameOps *ops, void (*log)(const char *line));
void pc5_game_ps5_ops(Pc5GameOps *ops, void (*log)(const char *line))
{
    *ops = (Pc5GameOps){ light_bar, light_bar_reset, triggers, cursor, log };
}
