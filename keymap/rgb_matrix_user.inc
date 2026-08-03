// Custom RGB Matrix effects.
//
// CLAUDE_BLACKOUT exists for a specific reason. rgb_matrix.c:421 reads:
//
//     case RENDERING:
//         rgb_task_render(effect);
//         if (effect) { ... rgb_matrix_indicators_advanced(...); }
//
// so when the matrix is disabled - or set to RGB_MATRIX_NONE - the effect index
// is 0 and the indicator callback is skipped entirely, taking the three Claude
// lamps down with the key backlight. A non-zero effect that simply renders
// black keeps the indicator path alive. That is what RM_TOGG selects now.

RGB_MATRIX_EFFECT(CLAUDE_BLACKOUT)
RGB_MATRIX_EFFECT(CLAUDE_AURA)
RGB_MATRIX_EFFECT(CLAUDE_RAIN)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#    include "claude_led.h"

// --- lights out, indicators still alive -----------------------------------
static bool CLAUDE_BLACKOUT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        rgb_matrix_set_color(i, 0, 0, 0);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// --- the board mirrors the three lamps ------------------------------------
// Same colour, same brightness, same instant: the board is split into thirds
// and each third takes the brightness of the lamp above it. A chase sweeps
// across the keys exactly as it sweeps across the top bar.
static bool CLAUDE_AURA(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint8_t r, g, b, v[3];
    bool    live = claude_current_frame(&r, &g, &b, v);

    // Nothing to report: behave like an ordinary solid backlight in whatever
    // colour is configured, rather than going black and looking broken.
    hsv_t hsv = rgb_matrix_config.hsv;
    rgb_t amb = hsv_to_rgb(hsv);

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        if (!live) {
            rgb_matrix_set_color(i, amb.r, amb.g, amb.b);
            continue;
        }
        // key LEDs span x roughly 0..207; split into three bands so each third
        // of the board follows the lamp above it
        uint8_t x    = g_led_config.point[i].x;
        uint8_t band = (x < 70) ? 0 : (x < 140 ? 1 : 2);
        uint8_t s    = scale8(v[band], rgb_matrix_config.hsv.v);
        rgb_matrix_set_color(i, scale8(r, s), scale8(g, s), scale8(b, s));
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// --- Matrix rain, coloured by state, seeded by what you type ---------------
// Per-column drop heads in 1/16 row units so they can fall at different speeds.
static uint8_t  rain_head[MATRIX_COLS];
static uint8_t  rain_speed[MATRIX_COLS];
static uint8_t  rain_row_of[RGB_MATRIX_LED_COUNT];
static uint8_t  rain_col_of[RGB_MATRIX_LED_COUNT];
static bool     rain_mapped = false;
static uint16_t rain_last;
static uint16_t rain_rng = 0xACE1;

static uint8_t rain_rand(void) {
    rain_rng ^= rain_rng << 7;
    rain_rng ^= rain_rng >> 9;
    rain_rng ^= rain_rng << 8;
    return (uint8_t)rain_rng;
}

// LED -> matrix position, so a drop can be placed on the exact key you hit.
static void rain_build_map(void) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rain_row_of[i] = 0xFF;
        rain_col_of[i] = 0xFF;
    }
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            uint8_t li = g_led_config.matrix_co[r][c];
            if (li != NO_LED && li < RGB_MATRIX_LED_COUNT) {
                rain_row_of[li] = r;
                rain_col_of[li] = c;
            }
        }
    }
    for (uint8_t c = 0; c < MATRIX_COLS; c++) {
        rain_head[c]  = rain_rand() % (MATRIX_ROWS * 16);
        rain_speed[c] = 2 + (rain_rand() % 3);
    }
    rain_mapped = true;
}

static bool CLAUDE_RAIN(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    if (!rain_mapped) {
        rain_build_map();
    }

    // Advance the simulation once per frame, not once per render block.
    if (params->iter == 0) {
        uint16_t now = timer_read();
        if ((uint16_t)(now - rain_last) > 28) {
            rain_last = now;
            for (uint8_t c = 0; c < MATRIX_COLS; c++) {
                // A keypress in this column drops a fresh bright head on it.
                if (claude_rain_kick[c]) {
                    rain_head[c]     = (uint8_t)((claude_rain_kick[c] - 1) * 16);
                    rain_speed[c]    = 2 + (rain_rand() % 3);
                    claude_rain_kick[c] = 0;
                } else {
                    rain_head[c] += rain_speed[c];
                    if (rain_head[c] >= (MATRIX_ROWS + 3) * 16) {
                        rain_head[c]  = 0;
                        rain_speed[c] = 2 + (rain_rand() % 3);
                    }
                }
            }
        }
    }

    uint8_t br, bg, bb;
    claude_state_rgb(claude_state, &br, &bg, &bb);

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();

        uint8_t row = rain_row_of[i];
        uint8_t col = rain_col_of[i];
        if (row == 0xFF || col >= MATRIX_COLS) {
            rgb_matrix_set_color(i, 0, 0, 0);
            continue;
        }

        int8_t  behind = (int8_t)(rain_head[col] >> 4) - (int8_t)row;
        uint8_t v;
        if (behind == 0) {
            v = 255; // the head is brightest
        } else if (behind == 1) {
            v = 90;
        } else if (behind == 2) {
            v = 30;
        } else if (behind == 3) {
            v = 10;
        } else {
            v = 0;
        }

        v = scale8(v, rgb_matrix_config.hsv.v);
        rgb_matrix_set_color(i, scale8(br, v), scale8(bg, v), scale8(bb, v));
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
