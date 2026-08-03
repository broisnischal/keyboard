// Shared between keymap.c and rgb_matrix_user.inc. The .inc is compiled inside
// rgb_matrix.c, so anything the effects need from the keymap has to be a real
// global declared here rather than a file-static.
#pragma once

#include <stdint.h>
#include <stdbool.h>

enum claude_state {
    CLAUDE_OFF        = 0, // no session
    CLAUDE_IDLE       = 1, // waiting for your prompt   - green
    CLAUDE_WORKING    = 2, // thinking / generating     - blue
    CLAUDE_LOADING    = 3, // running a tool            - green
    CLAUDE_PERMISSION = 4, // needs your approval       - red
    CLAUDE_DONE       = 5, // finished                  - green
    CLAUDE_ERROR      = 6, // failed                    - red
};

// Saturated only - at least one channel at zero, or it reads as white through
// the diffuser. This is the whole reason the first version looked wrong.
#define CLAUDE_RGB_GREEN 0, 255, 0
#define CLAUDE_RGB_BLUE  0, 60, 255
#define CLAUDE_RGB_RED   255, 0, 0

extern uint8_t claude_state;

// This frame's colour and the three lamp brightnesses. Shared so the board
// effect and the top-bar lamps animate as one thing, not two.
// Returns false when no slot is live, so callers can fall back to an ambient
// look instead of rendering a dead black board.
bool claude_current_frame(uint8_t *out_r, uint8_t *out_g, uint8_t *out_b, uint8_t v[3]);

// Set by process_record_kb on every keypress: value is (matrix row + 1) for the
// column that was just hit, 0 when consumed. Lets the rain effect spawn a drop
// exactly where you typed, without going through the hit tracker's pixel coords.
extern uint8_t claude_rain_kick[MATRIX_COLS];

// Base colour for the current state, written into r/g/b.
static inline void claude_state_rgb(uint8_t st, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (st) {
        case CLAUDE_WORKING:
            *r = 0;   *g = 60;  *b = 255; break;
        case CLAUDE_PERMISSION:
        case CLAUDE_ERROR:
            *r = 255; *g = 0;   *b = 0;   break;
        case CLAUDE_IDLE:
        case CLAUDE_LOADING:
        case CLAUDE_DONE:
            *r = 0;   *g = 255; *b = 0;   break;
        default:
            *r = 0;   *g = 90;  *b = 120; break; // no session: cool, quiet
    }
}
