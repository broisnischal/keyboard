/* Copyright 2025 Carlos Eduardo de Paula <carlosedp@gmail.com>
 * Copyright 2025 EPOMAKER <https://github.com/Epomaker>
 * Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
 * Copyright 2021 QMK <https://github.com/qmk/qmk_firmware>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "rdmctmzt_common.h"
#include "keyboard_common.h"
#include "bootloader.h"
#include "secure.h"
#include "claude_led.h"
#ifdef VIA_ENABLE
#    include "via.h"
#endif

// ===========================================================================
// Persistent settings (EEPROM)
//
// Four bytes of user datablock. Note that VIA derives its EEPROM magic from
// QMK_BUILDDATE, so *every reflash resets these to the defaults below* - they
// persist across unplugging and reboots, not across firmware updates.
// ===========================================================================

typedef struct __attribute__((packed)) {
    uint8_t lock_on_boot;   // require the unlock pattern after every power-up
    uint8_t claude_leds;    // drive the three indicator LEDs from Claude Code
    uint8_t led_brightness; // ceiling for the Claude animations, 0-255
    uint8_t saved_rgb_mode; // mode to restore when un-blanking the backlight
} user_config_t;

static user_config_t user_config;

void eeconfig_init_user(void) {
    // Defaults are deliberately safe: lock_on_boot OFF, so a bad unlock pattern
    // can never leave you with a keyboard you cannot type on.
    user_config.lock_on_boot   = 0;
    user_config.claude_leds    = 1;
    user_config.led_brightness = 255;
    user_config.saved_rgb_mode = RGB_MATRIX_SOLID_COLOR;
    eeconfig_update_user_datablock(&user_config, 0, sizeof(user_config));
}

// Deferred write. Holding a brightness key fires many changes a second, and
// this board's EEPROM is emulated in flash - committing each one stalls the
// matrix scan and burns write cycles. Mark dirty, flush once things settle.
static bool     cfg_dirty    = false;
static uint32_t cfg_dirty_at = 0;

static void user_config_save(void) {
    cfg_dirty    = true;
    cfg_dirty_at = timer_read32();
}

static void user_config_flush_if_due(void) {
    if (cfg_dirty && timer_elapsed32(cfg_dirty_at) > 750) {
        cfg_dirty = false;
        eeconfig_update_user_datablock(&user_config, 0, sizeof(user_config));
    }
}

void housekeeping_task_kb(void) {
    user_config_flush_if_due();
    housekeeping_task_user();
}

// This keymap hooks the *_kb variants throughout. th40.c owns the *_user ones,
// so overriding _kb keeps the keyboard file completely unmodified - nothing to
// reapply after a git pull. Every _kb override below calls through to _user.

enum layers {
    _BASE = 0,
    _NAV,   // 1  F-keys, arrows, browser
    _NUM,   // 2  digits and the common symbols
    _MEDIA, // 3  transport + volume
    _CODE,  // 4  operators, digraphs, paired delimiters
    _WM,    // 5  Hyprland workspaces and windows
    _GIT,   // 6  git / shell macros
    _HRM,   // 7  same base, but with home row mods - opt-in via UC_HRM
};

// ===========================================================================
// Macro strings
//
// EDIT ME: these are a first guess at the commands worth a single key. Change
// the strings, the names stay valid.
// ===========================================================================

enum custom_keycodes {
    // Operators and digraphs
    M_ARROW = SAFE_RANGE, // ->
    M_FATAR,              // =>
    M_NEQ,                // !=
    M_EQEQ,               // ==
    M_AND,                // &&
    M_OR,                 // ||
    M_LTE,                // <=
    M_GTE,                // >=
    M_SCOPE,              // ::
    // Paired delimiters, cursor left between them
    M_PAREN,              // ()
    M_BRACK,              // []
    M_BRACE,              // {}
    M_QUOT2,              // ""
    // git
    G_STATUS, G_ADD, G_COMMIT, G_PUSH, G_PULL, G_LOG, G_DIFF, G_CO, G_BRANCH, G_STASH,
    // shell
    S_CLAUDE, S_CLAUDEC, S_LAZYGIT, S_CDUP, S_LS,
    // persistent settings
    UC_LOCKB, // toggle "require the unlock pattern at power-up"
    UC_CLEDS, // toggle the Claude indicator LEDs
    UC_BRTU,  // Claude LED brightness up
    UC_BRTD,  // Claude LED brightness down
    UC_HRM,   // switch the default layer to the home-row-mod base (persisted)
    UC_AURA,  // whole-board Claude-reactive effect
    UC_RAIN,  // Matrix rain, seeded by typing
};

static bool send_macro(uint16_t keycode) {
    switch (keycode) {
        case M_ARROW:   SEND_STRING("->");                      return true;
        case M_FATAR:   SEND_STRING("=>");                      return true;
        case M_NEQ:     SEND_STRING("!=");                      return true;
        case M_EQEQ:    SEND_STRING("==");                      return true;
        case M_AND:     SEND_STRING("&&");                      return true;
        case M_OR:      SEND_STRING("||");                      return true;
        case M_LTE:     SEND_STRING("<=");                      return true;
        case M_GTE:     SEND_STRING(">=");                      return true;
        case M_SCOPE:   SEND_STRING("::");                      return true;
        case M_PAREN:   SEND_STRING("()" SS_TAP(X_LEFT));       return true;
        case M_BRACK:   SEND_STRING("[]" SS_TAP(X_LEFT));       return true;
        case M_BRACE:   SEND_STRING("{}" SS_TAP(X_LEFT));       return true;
        case M_QUOT2:   SEND_STRING("\"\"" SS_TAP(X_LEFT));     return true;

        case G_STATUS:  SEND_STRING("git status\n");            return true;
        case G_ADD:     SEND_STRING("git add -A\n");            return true;
        case G_COMMIT:  SEND_STRING("git commit -m \"");        return true;
        case G_PUSH:    SEND_STRING("git push\n");              return true;
        case G_PULL:    SEND_STRING("git pull\n");              return true;
        case G_LOG:     SEND_STRING("git log --oneline -15\n"); return true;
        case G_DIFF:    SEND_STRING("git diff\n");              return true;
        case G_CO:      SEND_STRING("git checkout ");           return true;
        case G_BRANCH:  SEND_STRING("git branch\n");            return true;
        case G_STASH:   SEND_STRING("git stash\n");             return true;

        case S_CLAUDE:  SEND_STRING("claude\n");                return true;
        case S_CLAUDEC: SEND_STRING("claude --continue\n");     return true;
        case S_LAZYGIT: SEND_STRING("lazygit\n");               return true;
        case S_CDUP:    SEND_STRING("cd ..\n");                 return true;
        case S_LS:      SEND_STRING("ls -la\n");                return true;
    }
    return false;
}

// ===========================================================================
// Home row mods (GACS) and the shift tap dance
// ===========================================================================

#define HM_A LGUI_T(KC_A)
#define HM_S LALT_T(KC_S)
#define HM_D LCTL_T(KC_D)
#define HM_F LSFT_T(KC_F)
#define HM_J RSFT_T(KC_J)
#define HM_K RCTL_T(KC_K)
#define HM_L LALT_T(KC_L)

enum tap_dance_index {
    TD_SFT_CAPS,
};

#define TD_SFT TD(TD_SFT_CAPS)

// Register shift on the keydown itself, so shifted typing gains no latency.
static void td_shift_each_tap(tap_dance_state_t *state, void *user_data) {
    if (state->count <= 2) {
        register_code(KC_LSFT);
    }
}

// Release it on the physical keyup, so it can never linger into the next keystroke.
static void td_shift_each_release(tap_dance_state_t *state, void *user_data) {
    unregister_code(KC_LSFT);
}

// Two taps that end with the key up (nothing interrupted them) toggle Caps Lock.
static void td_shift_finished(tap_dance_state_t *state, void *user_data) {
    if (state->count == 2 && !state->pressed) {
        tap_code(KC_CAPS);
    }
}

static void td_shift_reset(tap_dance_state_t *state, void *user_data) {
    unregister_code(KC_LSFT);
}

tap_dance_action_t tap_dance_actions[] = {
    [TD_SFT_CAPS] = ACTION_TAP_DANCE_FN_ADVANCED_WITH_RELEASE(td_shift_each_tap, td_shift_each_release, td_shift_finished, td_shift_reset),
};

// The global 130ms is what the LT() keys were tuned to; only the keys that
// carry a second job get a longer window.
uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case TD_SFT:
            return 200;
        case HM_A: case HM_S: case HM_D: case HM_F:
        case HM_J: case HM_K: case HM_L:
            return 180;
        default:
            return TAPPING_TERM;
    }
}

// Which hand each key belongs to, for CHORDAL_HOLD. Same-hand chords settle as
// taps; '*' keys (the three thumbs) may chord with either hand.
// clang-format off
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT_tkl_ansi(
    'L', 'L', 'L', 'L', 'L', 'L', 'R', 'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L', 'L', 'L', 'L', 'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L', 'L', 'L', 'L', 'L', 'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L',           '*', '*', '*', 'R', 'R', 'R'
);
// clang-format on

// Flow Tap is what makes home row mods feel like plain keys: while you are in a
// typing flow it settles a mod-tap as a tap on the KEYDOWN, instead of waiting
// for the release to find out which you meant.
//
// The two roles are NOT symmetric, and conflating them is what made typing drag:
//
//   - as the key being pressed, a layer-tap must never be flow-tapped, or the
//     space-layers die whenever you reach for one right after a letter;
//   - as the PREVIOUS key, a layer-tap absolutely must count as typing -
//     LT(2,KC_SPC) is the space bar, so excluding it broke the flow chain after
//     every single space, and the first letter of every word went back to
//     resolving on release.
//
// get_flow_tap_term() takes precedence over is_flow_tap_key(), so this is the
// only hook needed.

// Does this key mean "the user is mid-flow"? Judged on the tap keycode, so the
// space bar counts even though it is really LT(2,KC_SPC).
static bool flow_prev_is_typing(uint16_t keycode) {
    if (IS_QK_LAYER_TAP(keycode)) {
        keycode = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    } else if (IS_QK_MOD_TAP(keycode)) {
        keycode = QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
    }
    switch (keycode) {
        case KC_A ... KC_Z:
        case KC_1 ... KC_0:
        case KC_SPC:
        case KC_DOT:
        case KC_COMM:
        case KC_SCLN:
        case KC_SLSH:
        case KC_QUOT:
        case KC_MINS:
        case KC_UNDS:
        case KC_BSPC:
            return true;
    }
    return false;
}

uint16_t get_flow_tap_term(uint16_t keycode, keyrecord_t *record, uint16_t prev_keycode) {
    if (IS_QK_LAYER_TAP(keycode)) {
        return 0; // a layer hold must always be reachable
    }
    if (!IS_QK_MOD_TAP(keycode)) {
        return 0; // only the home row mods need rescuing
    }
    if ((get_mods() & (MOD_MASK_CG | MOD_BIT_LALT)) != 0) {
        return 0; // mid-hotkey, leave the hold alone
    }
    return flow_prev_is_typing(prev_keycode) ? FLOW_TAP_TERM : 0;
}

// ===========================================================================
// ===========================================================================
// Status bus
//
// Generalised from "show what Claude is doing" to "show what any process on the
// host wants to say". Four priority slots; the highest-priority live slot owns
// the three lamps. Slot 0 is Claude, slots 1-3 are for anything else - low
// disk, wifi down, a build that failed.
//
// Colour handling is deliberate, not decorative:
//   * Saturated hues only. A colour lighting all three channels reads as white
//     through the diffuser - that is what made the first palette look broken.
//   * Per-channel gain. A WS2812 green at full scale is perceptually several
//     times brighter than blue, so a "traffic light" built from raw 255s has a
//     blinding green and a murky blue. The gains below even them out.
//   * Gamma on the animation curve. PWM is linear, sight is not; a linear
//     breathe spends most of its time looking bright, which is what reads as
//     cheap. Squaring the curve makes it fade like a real light.
// ===========================================================================

enum bus_pattern {
    PAT_OFF = 0,
    PAT_SOLID,
    PAT_BREATH,  // slow, calm
    PAT_PULSE,   // faster, working
    PAT_CHASE,   // . . . across the three lamps
    PAT_BLINK,   // hard on/off, demands attention
    PAT_STROBE3, // three quick flashes, then hold
};

#define BUS_SLOTS 4
#define BUS_SLOT_CLAUDE 0

typedef struct {
    uint8_t  pattern;
    uint8_t  r, g, b;
    uint8_t  priority;
    uint16_t ttl_ms; // 0 = until replaced
    uint32_t set_at;
} bus_slot_t;

static bus_slot_t bus[BUS_SLOTS];

// Perceptual gains. Green is pulled well down, blue left at full - this is the
// single change that stops the palette looking like raw hex values.
// Mild hue balance ONLY. The previous values (G at 100) cut green to 39% and,
// stacked on gamma and the pattern floor, left idle at roughly 12% output -
// which is what "brightness too low" was. Green still needs trimming so it does
// not dominate red and amber, but nothing here should act as a volume control.
#define GAIN_R 255
#define GAIN_G 170
#define GAIN_B 255

uint8_t claude_state = CLAUDE_OFF;              // read by rgb_matrix_user.inc
uint8_t claude_rain_kick[MATRIX_COLS] = {0};    // ditto

#define CLAUDE_CMD 0xC1
#define CLAUDE_SUB_BOOTLOADER 0xB0
#define CLAUDE_SUB_SCANRATE   0xB1
#define CLAUDE_SUB_BUS_SET    0xB2
#define CLAUDE_SUB_BUS_CLEAR  0xB3
#define CLAUDE_SUB_CFG_GET    0xB4
#define CLAUDE_SUB_CFG_SET    0xB5
#define CLAUDE_SUB_UNLOCK     0xB6

#define CLAUDE_LED_A 44 // shared with the Caps Lock indicator
#define CLAUDE_LED_B 45
#define CLAUDE_LED_C 46

#define CLAUDE_WATCHDOG_MS 1800000 // 30 min silent -> assume the host died

static void bus_set(uint8_t slot, uint8_t pattern, uint8_t r, uint8_t g, uint8_t b, uint8_t priority, uint16_t ttl_ms) {
    if (slot >= BUS_SLOTS) return;
    bus[slot].pattern  = pattern;
    bus[slot].r        = r;
    bus[slot].g        = g;
    bus[slot].b        = b;
    bus[slot].priority = priority;
    bus[slot].ttl_ms   = ttl_ms;
    bus[slot].set_at   = timer_read32();
}

// Claude states are just a preset row in the bus. Amber replaces the old blue:
// blue on a diffused indicator reads dim and cold next to green and red, and no
// amount of gain fixes that - the hue itself was the problem.
static void bus_set_claude(uint8_t st) {
    claude_state = st;
    switch (st) {
        case CLAUDE_IDLE:       bus_set(BUS_SLOT_CLAUDE, PAT_BREATH,  0, 255,   0, 10,    0); break;
        case CLAUDE_WORKING:    bus_set(BUS_SLOT_CLAUDE, PAT_PULSE, 255, 120,   0, 20,    0); break;
        case CLAUDE_LOADING:    bus_set(BUS_SLOT_CLAUDE, PAT_CHASE, 255, 120,   0, 20,    0); break;
        case CLAUDE_PERMISSION: bus_set(BUS_SLOT_CLAUDE, PAT_BLINK, 255,   0,   0, 90,    0); break;
        case CLAUDE_DONE:       bus_set(BUS_SLOT_CLAUDE, PAT_STROBE3, 0, 255,   0, 60, 2200); break;
        case CLAUDE_ERROR:      bus_set(BUS_SLOT_CLAUDE, PAT_SOLID, 255,   0,   0, 80, 4000); break;
        default:                bus_set(BUS_SLOT_CLAUDE, PAT_OFF,     0,   0,   0,  0,    0); break;
    }
}

static bool bus_slot_live(const bus_slot_t *s) {
    if (s->pattern == PAT_OFF) return false;
    if (s->ttl_ms && timer_elapsed32(s->set_at) > s->ttl_ms) return false;
    return true;
}

#ifdef VIA_ENABLE
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (data[0] != CLAUDE_CMD) {
        return false; // not ours - let VIA handle it
    }
    switch (data[1]) {
        case CLAUDE_SUB_BOOTLOADER:
            raw_hid_send(data, length);
            bootloader_jump();
            return true;
        case CLAUDE_SUB_SCANRATE: {
            uint32_t rate = get_matrix_scan_rate();
            data[2] = (uint8_t)(rate & 0xFF);
            data[3] = (uint8_t)((rate >> 8) & 0xFF);
            raw_hid_send(data, length);
            return true;
        }
        case CLAUDE_SUB_BUS_SET:
            // slot, pattern, r, g, b, priority, ttl_seconds (16-bit LE)
            bus_set(data[2], data[3], data[4], data[5], data[6], data[7],
                    (uint16_t)((data[8] | (data[9] << 8)) * 1000));
            raw_hid_send(data, length);
            return true;
        case CLAUDE_SUB_BUS_CLEAR:
            if (data[2] < BUS_SLOTS) bus[data[2]].pattern = PAT_OFF;
            raw_hid_send(data, length);
            return true;
        case CLAUDE_SUB_CFG_GET:
            data[2] = user_config.lock_on_boot;
            data[3] = user_config.claude_leds;
            data[4] = user_config.led_brightness;
            data[5] = user_config.saved_rgb_mode;
            data[6] = secure_is_locked() ? 1 : 0;
            raw_hid_send(data, length);
            return true;
        case CLAUDE_SUB_CFG_SET:
            switch (data[2]) {
                case 0: user_config.lock_on_boot   = data[3] ? 1 : 0; break;
                case 1: user_config.claude_leds    = data[3] ? 1 : 0; break;
                case 2: user_config.led_brightness = data[3]; break;
            }
            user_config_save();
            raw_hid_send(data, length);
            return true;
        case CLAUDE_SUB_UNLOCK:
            // Escape hatch. The threat model is someone walking up to the
            // keyboard, not someone already sitting at the unlocked machine -
            // so being able to unlock from the host costs nothing and removes
            // the only way this feature could genuinely strand you.
            secure_unlock();
            raw_hid_send(data, length);
            return true;
        default:
            break;
    }
    if (data[1] <= CLAUDE_ERROR) {
        bus_set_claude(data[1]);
    }
    data[1] = claude_state;
    raw_hid_send(data, length);
    return true;
}
#endif

// Triangle wave over `period` ms, 0 -> 255 -> 0.
static uint8_t claude_tri(uint32_t now, uint32_t period) {
    uint32_t x    = now % period;
    uint32_t half = period / 2;
    return (x < half) ? (uint8_t)((x * 255) / half) : (uint8_t)(((period - x) * 255) / half);
}

// Gamma ~2.0. Cheap, and enough to make a fade look like a fade.
static uint8_t claude_gamma(uint8_t v) {
    return (uint8_t)(((uint16_t)v * v) / 255);
}

static uint8_t claude_scale(uint8_t c, uint8_t v) {
    return (uint8_t)(((uint16_t)c * v) / 255);
}

// Computes this frame's colour and the three lamp brightnesses. Both the lamps
// and the CLAUDE_AURA board effect call it, so the backlight blinks in exact
// step with the top bar instead of running its own animation.
bool claude_current_frame(uint8_t *out_r, uint8_t *out_g, uint8_t *out_b, uint8_t v[3]) {
    v[0] = v[1] = v[2] = 0;
    *out_r = *out_g = *out_b = 0;

    if (claude_state == CLAUDE_DONE || claude_state == CLAUDE_ERROR) {
        if (!bus_slot_live(&bus[BUS_SLOT_CLAUDE])) bus_set_claude(CLAUDE_IDLE);
    } else if (claude_state != CLAUDE_OFF && timer_elapsed32(bus[BUS_SLOT_CLAUDE].set_at) > CLAUDE_WATCHDOG_MS) {
        bus_set_claude(CLAUDE_OFF);
    }

    // Locked wins over everything. Without this a locked board is completely
    // indistinguishable from a broken one, which is how you end up reflashing
    // a keyboard that was working fine.
    if (secure_is_locked()) {
        uint32_t now = timer_read32();
        uint8_t  b   = 25 + claude_scale(90, claude_gamma(claude_tri(now, 2600)));
        v[0] = v[1] = v[2] = b;
        *out_r = claude_scale(255, GAIN_R);
        *out_g = 0;
        *out_b = claude_scale(140, GAIN_B);
        return true;
    }

    const bus_slot_t *win = NULL;
    for (uint8_t i = 0; i < BUS_SLOTS; i++) {
        if (!bus_slot_live(&bus[i])) continue;
        if (i == BUS_SLOT_CLAUDE && !user_config.claude_leds) continue;
        if (!win || bus[i].priority > win->priority) win = &bus[i];
    }
    if (!win) return false;

    uint32_t now = timer_read32();
    switch (win->pattern) {
        case PAT_SOLID:
            v[0] = v[1] = v[2] = 255;
            break;
        case PAT_BREATH: { // calm, but never so dim you wonder if it is broken
            uint8_t b = 55 + claude_scale(200, claude_gamma(claude_tri(now, 4200)));
            v[0] = v[1] = v[2] = b;
            break;
        }
        case PAT_PULSE: { // full-range breathing - this is the "working" state
            uint8_t b = 70 + claude_scale(185, claude_gamma(claude_tri(now, 1500)));
            v[0] = v[1] = v[2] = b;
            break;
        }
        case PAT_CHASE: {
            uint8_t head = (now / 260) % 3;
            v[head]           = 255;
            v[(head + 2) % 3] = 70;
            break;
        }
        case PAT_BLINK:
            if ((now / 150) % 2 == 0) v[0] = v[1] = v[2] = 255;
            break;
        case PAT_STROBE3: {
            uint32_t t = timer_elapsed32(win->set_at);
            uint8_t  b = (t < 900) ? (((t / 150) % 2) ? 0 : 255) : 230;
            v[0] = v[1] = v[2] = b;
            break;
        }
        default:
            break;
    }

    *out_r = claude_scale(win->r, GAIN_R);
    *out_g = claude_scale(win->g, GAIN_G);
    *out_b = claude_scale(win->b, GAIN_B);
    return true;
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    uint8_t r, g, b, v[3];
    claude_current_frame(&r, &g, &b, v);

    const uint8_t bright = user_config.led_brightness;
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t s   = claude_scale(v[i], bright);
        uint8_t idx = CLAUDE_LED_A + i;
        RGB_MATRIX_INDICATOR_SET_COLOR(idx, claude_scale(r, s), claude_scale(g, s), claude_scale(b, s));
    }

    // Caps Lock keeps LED 44 - th40.c's _user handler repaints it after us.
    return rgb_matrix_indicators_advanced_user(led_min, led_max);
}

// ===========================================================================
// Caps Word, combos, swap hands
// ===========================================================================

// Keep the word alive through the characters that appear inside identifiers,
// so SCREAMING_SNAKE_CASE and CONST-NAMES survive; anything else ends it.
bool caps_word_press_user(uint16_t keycode) {
    switch (keycode) {
        case KC_A ... KC_Z:
        case KC_MINS:
            add_weak_mods(MOD_BIT(KC_LSFT)); // shift these
            return true;
        case KC_1 ... KC_0:
        case KC_UNDS:
        case KC_BSPC:
        case KC_DEL:
            return true; // continue without shifting
        default:
            return false; // end Caps Word
    }
}

// Pairs chosen to be digraphs that essentially never occur in English, so a
// fast roll types the letters instead of firing the combo.
const uint16_t PROGMEM combo_esc[]   = {KC_Q, KC_W, COMBO_END};
const uint16_t PROGMEM combo_undo[]  = {KC_Z, KC_X, COMBO_END};
const uint16_t PROGMEM combo_caps[]  = {KC_C, KC_V, COMBO_END};
const uint16_t PROGMEM combo_del[]   = {KC_N, KC_M, COMBO_END};
const uint16_t PROGMEM combo_mins[]  = {KC_M, KC_COMM, COMBO_END};
const uint16_t PROGMEM combo_unds[]  = {KC_COMM, KC_DOT, COMBO_END};
const uint16_t PROGMEM combo_lock[]  = {KC_Q, KC_P, COMBO_END}; // opposite corners, two hands

combo_t key_combos[] = {
    COMBO(combo_esc,  KC_ESC),
    COMBO(combo_undo, LCTL(KC_Z)),
    COMBO(combo_caps, CW_TOGG),
    COMBO(combo_del,  KC_DEL),
    COMBO(combo_mins, KC_MINS),
    COMBO(combo_unds, KC_UNDS),
    COMBO(combo_lock, QK_SECURE_LOCK),
};

// One-handed mode: mirror each row across its own centre. The rows have
// different key spans on this board, so a single "11 - col" would map several
// keys onto holes.
// clang-format off
const keypos_t PROGMEM hand_swap_config[MATRIX_ROWS][MATRIX_COLS] = {
    // row 0: one key, maps to itself
    {{0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8},{0,9},{0,10},{0,11}},
    // row 1: keys at cols 1-11  -> col = 12 - col
    {{1,0},{1,11},{1,10},{1,9},{1,8},{1,7},{1,6},{1,5},{1,4},{1,3},{1,2},{1,1}},
    // row 2: keys at cols 0-10  -> col = 10 - col
    {{2,10},{2,9},{2,8},{2,7},{2,6},{2,5},{2,4},{2,3},{2,2},{2,1},{2,0},{2,11}},
    // row 3: keys at cols 0-11  -> col = 11 - col
    {{3,11},{3,10},{3,9},{3,8},{3,7},{3,6},{3,5},{3,4},{3,3},{3,2},{3,1},{3,0}},
    // row 4: sparse (0,2,3,4,6,7,8,9,10) -> mirrored within that set
    {{4,10},{4,1},{4,9},{4,8},{4,7},{4,5},{4,6},{4,4},{4,3},{4,2},{4,0},{4,11}},
};
// clang-format on

// ===========================================================================
// Init and key processing
// ===========================================================================

void keyboard_post_init_kb(void) {
    eeconfig_read_user_datablock(&user_config, 0, sizeof(user_config));
    rgb_matrix_enable_noeeprom(); // indicators only render while the matrix runs
    if (rgb_matrix_get_mode() == RGB_MATRIX_NONE) {
        // rgb_matrix.c skips the indicator callback whenever the effect index is
        // 0, so NONE would silently kill the lamps. CLAUDE_BLACKOUT looks
        // identical (all keys dark) but is a real effect, so indicators live.
        rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_CLAUDE_AURA);
    }
    // Boot showing something. Coming up in CLAUDE_OFF meant the lamps were dark
    // and - with the Aura effect selected - the whole board was dark too, which
    // is indistinguishable from broken after a replug.
    bus_set_claude(CLAUDE_IDLE);
    if (!user_config.lock_on_boot) {
        secure_unlock(); // Secure boots LOCKED; opt in before we honour that
    }
    keyboard_post_init_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // While locked, swallow everything. The top-left key is the only way in -
    // matched on its matrix position so it works regardless of the active
    // layer, since layer keys are swallowed too.
    if (secure_is_locked()) {
        if (record->event.pressed && record->event.key.row == 0 && record->event.key.col == 0) {
            secure_request_unlock(); // now type the pattern; core handles the rest
        }
        return false;
    }

    if (record->event.pressed && record->event.key.col < MATRIX_COLS) {
        // Seed the rain effect at the exact key that was hit.
        claude_rain_kick[record->event.key.col] = record->event.key.row + 1;
    }

    if (record->event.pressed) {
        switch (keycode) {
            case RM_TOGG:
                // Never actually disable the matrix: effect 0 makes rgb_matrix.c
                // skip the indicator callback, which would take the Claude lamps
                // out with the backlight. Swap to a black effect instead.
                if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_CLAUDE_BLACKOUT) {
                    uint8_t cur = rgb_matrix_get_mode();
                    user_config.saved_rgb_mode = cur ? cur : RGB_MATRIX_SOLID_COLOR;
                    user_config_save();
                    rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_CLAUDE_BLACKOUT);
                } else {
                    rgb_matrix_mode_noeeprom(user_config.saved_rgb_mode ? user_config.saved_rgb_mode : RGB_MATRIX_SOLID_COLOR);
                }
                return false;
            case UC_AURA:
                rgb_matrix_mode(RGB_MATRIX_CUSTOM_CLAUDE_AURA);
                return false;
            case UC_RAIN:
                rgb_matrix_mode(RGB_MATRIX_CUSTOM_CLAUDE_RAIN);
                return false;
            case UC_LOCKB:
                user_config.lock_on_boot = !user_config.lock_on_boot;
                user_config_save();
                return false;
            case UC_CLEDS:
                user_config.claude_leds = !user_config.claude_leds;
                user_config_save();
                return false;
            case UC_BRTU:
                user_config.led_brightness = (user_config.led_brightness > 223) ? 255 : user_config.led_brightness + 32;
                user_config_save();
                return false;
            case UC_HRM: {
                // Swap the default layer rather than trying to defeat tap-hold
                // at runtime: _HRM is the same base with plain letters.
                // set_single_persistent_default_layer() writes it to EEPROM.
                uint8_t next = (get_highest_layer(default_layer_state) == _HRM) ? _BASE : _HRM;
                set_single_persistent_default_layer(next);
                return false;
            }
            case UC_BRTD:
                // floor at 16 rather than 0, so this can never look like a fault
                user_config.led_brightness = (user_config.led_brightness < 48) ? 16 : user_config.led_brightness - 32;
                user_config_save();
                return false;
        }
        if (send_macro(keycode)) {
            return false;
        }
    }
    return process_record_user(keycode, record);
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // Base. Plain letters - home row mods live on _HRM (layer 7) instead.
    [_BASE] = LAYOUT_tkl_ansi(
        QK_GESC     , KC_Q        , KC_W        , KC_E        , KC_R        , KC_T        , KC_Y        , KC_U        , KC_I        , KC_O        , KC_P        , KC_BSPC,
        LT(1,KC_TAB), KC_A        , KC_S        , KC_D        , KC_F        , KC_G        , KC_H        , KC_J        , KC_K        , KC_L        , KC_ENT,
        TD_SFT      , OSL(2)      , KC_Z        , KC_X        , KC_C        , KC_V        , KC_B        , KC_N        , KC_M        , KC_COMM     , KC_DOT      , KC_QUOT,
        KC_LCTL     , KC_LGUI     , KC_LALT     , LT(2,KC_SPC), LT(3,KC_SPC), LT(1,KC_SPC), KC_F21      , KC_F22      , KC_F23
    ),
    // Nav / F-keys. Bottom-right cluster reaches the three new layers.
    [_NAV] = LAYOUT_tkl_ansi(
        QK_GESC     , KC_F1       , KC_F2       , KC_F3       , KC_F4       , KC_F5       , KC_F6       , KC_F7       , KC_F8       , KC_F9       , KC_F10      , KC_DEL,
        _______     , KC_HOME     , KC_PGDN     , KC_PGUP     , KC_END      , KC_INS      , KC_LEFT     , KC_DOWN     , KC_UP       , KC_RIGHT    , KC_ENT,
        TD_SFT      , _______     , KC_PSCR     , CG_TOGG     , KC_WBAK     , KC_WFWD     , QK_REP      , QK_AREP     , XXXXXXX     , KC_F11      , KC_F12      , MO(0),
        _______     , _______     , _______     , _______     , _______     , LT(2,KC_SPC), MO(_CODE)   , MO(_WM)     , MO(_GIT)
    ),
    // Digits and everyday symbols.
    [_NUM] = LAYOUT_tkl_ansi(
        QK_GESC   , KC_1      , KC_2      , KC_3      , KC_4      , KC_5      , KC_6      , KC_7      , KC_8      , KC_9      , KC_0      , KC_BSPC,
        _______   , KC_MINS   , KC_EQL    , KC_SCLN   , KC_QUOT   , KC_GRV    , KC_LBRC   , KC_RBRC   , KC_SLSH   , S(KC_SLSH), KC_ENT,
        TD_SFT    , _______   , _______   , _______   , _______   , _______   , _______   , _______   , KC_NUBS   , KC_COMM   , KC_DOT    , MO(0),
        _______   , _______   , _______   , _______   , _______   , KC_SPC    , _______   , _______   , _______
    ),
    // Media + system. Settings on the top row persist to EEPROM; the home row
    // holds lock / one-handed / Caps Word next to the transport keys, and the
    // bottom row is one-shot mods.
    [_MEDIA] = LAYOUT_tkl_ansi(
        _______, UC_LOCKB     , UC_CLEDS     , UC_BRTD      , UC_BRTU      , UC_HRM , UC_AURA, UC_RAIN, RM_TOGG, XXXXXXX, XXXXXXX, _______,
        _______, SE_LOCK      , SH_TOGG      , CW_TOGG      , QK_REP       , KC_MPRV, KC_MPLY, KC_MNXT, KC_VOLD, KC_VOLU, _______,
        _______, OSM(MOD_LSFT), OSM(MOD_LCTL), OSM(MOD_LALT), OSM(MOD_LGUI), XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, MO(0),
        _______, _______      , _______      , _______      , _______      , _______, _______, _______, _______
    ),
    // Code: operators on the number row, digraphs on the home row, delimiters below.
    [_CODE] = LAYOUT_tkl_ansi(
        _______, KC_EXLM, KC_AT  , KC_HASH, KC_DLR , KC_PERC, KC_CIRC, KC_AMPR, KC_ASTR, KC_MINS, KC_PLUS, _______,
        _______, M_ARROW, M_FATAR, M_NEQ  , M_EQEQ , M_AND  , M_OR   , M_PAREN, M_BRACK, M_BRACE, _______,
        _______, KC_TILD, KC_GRV , KC_BSLS, KC_PIPE, KC_LABK, KC_RABK, M_LTE  , M_GTE  , M_SCOPE, M_QUOT2, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______
    ),
    // Hyprland. Sends the chords omarchy already binds, so no WM config needed.
    [_WM] = LAYOUT_tkl_ansi(
        _______, LGUI(KC_1)       , LGUI(KC_2)       , LGUI(KC_3)       , LGUI(KC_4)       , LGUI(KC_5)       , LGUI(KC_6)       , LGUI(KC_7)       , LGUI(KC_8)       , LGUI(KC_9)       , LGUI(KC_0), _______,
        _______, LGUI(LSFT(KC_1)) , LGUI(LSFT(KC_2)) , LGUI(LSFT(KC_3)) , LGUI(LSFT(KC_4)) , LGUI(LSFT(KC_5)) , LGUI(LSFT(KC_6)) , LGUI(LSFT(KC_7)) , LGUI(LSFT(KC_8)) , LGUI(LSFT(KC_9)) , _______,
        _______, LGUI(KC_W)       , LGUI(KC_F)       , LGUI(KC_LEFT)    , LGUI(KC_DOWN)    , LGUI(KC_UP)      , LGUI(KC_RIGHT)   , LGUI(KC_J)       , LGUI(KC_P)       , LGUI(KC_C)       , LGUI(KC_V), _______,
        _______, _______          , _______          , _______          , _______          , _______          , _______          , _______          , _______
    ),
    // git and shell one-shots.
    [_GIT] = LAYOUT_tkl_ansi(
        _______, G_STATUS , G_ADD    , G_COMMIT , G_PUSH  , G_PULL, G_LOG  , G_DIFF , G_CO   , G_BRANCH, G_STASH, _______,
        _______, S_CLAUDE , S_CLAUDEC, S_LAZYGIT, S_CDUP  , S_LS  , XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX , _______,
        _______, XXXXXXX  , XXXXXXX  , XXXXXXX  , XXXXXXX , XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, MO(0),
        _______, _______  , _______  , _______ , _______, _______, _______, _______, _______
    ),
    // Identical to _BASE but with home row mods on ASDF / JKL. UC_HRM swaps the
    // default layer here and persists it, so they are opt-in without reflashing.
    [_HRM] = LAYOUT_tkl_ansi(
        QK_GESC     , KC_Q        , KC_W        , KC_E        , KC_R        , KC_T        , KC_Y        , KC_U        , KC_I        , KC_O        , KC_P        , KC_BSPC,
        LT(1,KC_TAB), HM_A        , HM_S        , HM_D        , HM_F        , KC_G        , KC_H        , HM_J        , HM_K        , HM_L        , KC_ENT,
        TD_SFT      , OSL(2)      , KC_Z        , KC_X        , KC_C        , KC_V        , KC_B        , KC_N        , KC_M        , KC_COMM     , KC_DOT      , KC_QUOT,
        KC_LCTL     , KC_LGUI     , KC_LALT     , LT(2,KC_SPC), LT(3,KC_SPC), LT(1,KC_SPC), KC_F21      , KC_F22      , KC_F23
    )
};
// clang-format on
