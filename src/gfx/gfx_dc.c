#if defined(TARGET_DC)
#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"
#include "macros.h"
#include <kos.h>
#include <stdio.h>
#include <string.h>
#include <dc/video.h>
#include <assert.h>

#define GFX_API_NAME "Dreamcast GLdc"
#define SCR_WIDTH (640)
#define SCR_HEIGHT (480)

int force_30fps = 1;   // 0 = uncapped (real vsync, 60Hz-capable) — only 1P racing sets this.
                       // 1 = vblank-locked 30 (menus/split-screen/boot default).
                       // Owned by game code: update_gamestate() defaults 1 on every state
                       // change; race_logic_loop re-asserts per mode every frame.
static unsigned int last_time = 0;

extern void glKosSwapBuffers(void);
extern uint64_t timer_ms_gettime64(void);

unsigned int GetSystemTimeLow(void) {
    uint64_t msec = timer_ms_gettime64();
    return (unsigned int) msec;
}

void DelayThread(unsigned int ms) {
    thd_sleep(ms);
}

static void gfx_dc_init(UNUSED const char *game_name, UNUSED uint8_t start_in_fullscreen) {
    last_time = GetSystemTimeLow();
}

static void gfx_dc_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(uint8_t is_now_fullscreen)) {
}

static void gfx_dc_set_fullscreen(UNUSED uint8_t enable) {
}

static void gfx_dc_set_keyboard_callbacks(UNUSED uint8_t (*on_key_down)(int scancode),
                                          UNUSED uint8_t (*on_key_up)(int scancode),
                                          UNUSED void (*on_all_keys_up)(void)) {
}

static void gfx_dc_main_loop(void (*run_one_game_iter)(void)) {
    while (1) {
        run_one_game_iter();
    }
}

static void gfx_dc_get_dimensions(uint32_t *width, uint32_t *height) {
    *width = SCR_WIDTH;
    *height = SCR_HEIGHT;
}

/* What events should we be handling? */
static void gfx_dc_handle_events(void) {
    /* Lets us yield to other threads*/
    //DelayThread(100);
}

uint8_t skip_debounce = 0;
const unsigned int FRAME_TIME_MS = 33; // hopefully get right on target @ 33.3

static uint8_t gfx_dc_start_frame(void) {
    // The old ms-based frame-skip is GONE (2026-08-18). It depended on last_time being
    // refreshed by the old sleep-cap in swap_buffers_end; with that replaced by vblank
    // pacing, last_time froze at boot, elapsed was always > 33ms, and every OTHER frame
    // "skipped": logic still ran, render didn't, and the skipped frame bypassed the pacer
    // (dropped_frame skips swap_buffers_end) -> two logic frames per 2-vblank beat =
    // DOUBLE GAME SPEED at a correct-looking 30fps. With vblank pacing a slow frame
    // self-paces (the +2 target resyncs), so skipping is unnecessary — always render.
    return 1;
}

static void gfx_dc_swap_buffers_begin(void) {
}

extern volatile uint64_t vblticker;
static uint64_t last_ticker = 0;
static void gfx_dc_swap_buffers_end(void) {

    // 30fps pacing on the real 60Hz vblank: hold each frame to a CONSTANT 2-vblank beat.
    // NEVER pace on gTickSpeed/sNumVBlanks — those MEASURE the frame we're pacing, so a
    // one-frame hiccup ratchets the wait up permanently (wait 3 -> measures 3 -> wait 3...).
    // A slow frame (vblticker already past target) resyncs instead of running a debt.
    // NB: vblfunc must genwait_wake_ALL — the audio thread sleeps on this channel too, and
    // wake_one only ever woke one of us per vblank (starving the other).
    if (force_30fps) {
        const uint64_t target = last_ticker + 2;
        while (vblticker < target)
            genwait_wait((void*)&vblticker, NULL, 0, NULL);
        last_ticker = (vblticker > target) ? vblticker : target;
    }


    // Number of microseconds a frame should take (30 fps)
/*    const unsigned int cur_time = GetSystemTimeLow();
    const unsigned int elapsed = cur_time - last_time;
    last_time = cur_time;

    if (force_30fps && elapsed < FRAME_TIME_MS) {
#ifdef DEBUG
        printf("elapsed %d ms fps %f delay %d \n", elapsed, 1000.0f / elapsed, FRAME_TIME_MS - elapsed);
#endif
        DelayThread(FRAME_TIME_MS - elapsed);
        last_time += (FRAME_TIME_MS - elapsed);
    }*/

    /* Lets us yield to other threads*/
#ifndef GFX_BACKEND_PVR
    glKosSwapBuffers();
#endif
    /* PVR backend (gfx_pvr.c) flips inside pvr_scene_finish (finish_render). */
}

/* Idk what this is for? */
static double gfx_dc_get_time(void) {
    return 0.0;
}

struct GfxWindowManagerAPI gfx_dc = { gfx_dc_init,
                                      gfx_dc_set_keyboard_callbacks,
                                      gfx_dc_set_fullscreen_changed_callback,
                                      gfx_dc_set_fullscreen,
                                      gfx_dc_main_loop,
                                      gfx_dc_get_dimensions,
                                      gfx_dc_handle_events,
                                      gfx_dc_start_frame,
                                      gfx_dc_swap_buffers_begin,
                                      gfx_dc_swap_buffers_end,
                                      gfx_dc_get_time };


#endif // TARGET_DC
