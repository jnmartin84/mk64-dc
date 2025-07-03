#ifndef GFX_WINDOW_MANAGER_API_H
#define GFX_WINDOW_MANAGER_API_H

#include <stdint.h>
//#include <stduint8_t.h>

struct GfxWindowManagerAPI {
    void (*init)(const char *game_name, uint8_t start_in_fullscreen);
    void (*set_keyboard_callbacks)(uint8_t (*on_key_down)(int scancode), uint8_t (*on_key_up)(int scancode), void (*on_all_keys_up)(void));
    void (*set_fullscreen_changed_callback)(void (*on_fullscreen_changed)(uint8_t is_now_fullscreen));
    void (*set_fullscreen)(uint8_t enable);
    void (*main_loop)(void (*run_one_game_iter)(void));
    void (*get_dimensions)(uint32_t *width, uint32_t *height);
    void (*handle_events)(void);
    uint8_t (*start_frame)(void);
    void (*swap_buffers_begin)(void);
    void (*swap_buffers_end)(void);
    double (*get_time)(void); // For debug
};

#endif
