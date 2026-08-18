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

// The keyboard's config.h caps the datablock at 4 bytes, hence the bitfields.
typedef struct __attribute__((packed)) {
    uint8_t lock_on_boot   : 1; // require the unlock pattern after every power-up
    uint8_t claude_leds    : 1; // drive the three indicator LEDs from Claude Code
    uint8_t autoshift_on   : 1; // Auto Shift trades hold-latency for shifts - opt-in
    // (Autocorrect is NOT here: its state lives in keymap_config, which QMK
    // persists itself - defaulting to on via eeconfig.c.)
    uint8_t led_brightness;     // ceiling for the Claude animations, 0-255
    uint8_t saved_rgb_mode;     // mode to restore when un-blanking the backlight
} user_config_t;

static user_config_t user_config;

void eeconfig_init_user(void) {
    // Defaults are deliberately safe: lock_on_boot OFF, so a bad unlock pattern
    // can never leave you with a keyboard you cannot type on.
    user_config.lock_on_boot   = 0;
    user_config.claude_leds    = 1;
    user_config.led_brightness = 255;
    user_config.saved_rgb_mode = RGB_MATRIX_SOLID_COLOR;
    user_config.autoshift_on   = 0;
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

// AS_TOGG is consumed inside QMK, so the new state is only visible after the
// fact. Poll-and-compare here; the deferred write above coalesces it.
// (Autocorrect needs none of this - AC_TOGG persists via keymap_config.)
static void feature_state_sync(void) {
#ifdef AUTO_SHIFT_ENABLE
    uint8_t as = get_autoshift_state() ? 1 : 0;
    if (as != user_config.autoshift_on) {
        user_config.autoshift_on = as;
        user_config_save();
    }
#endif
}

void housekeeping_task_kb(void) {
    feature_state_sync();
    user_config_flush_if_due();
    housekeeping_task_user();
}

// This keymap hooks the *_kb variants throughout. th40.c owns the *_user ones,
// so overriding _kb keeps the keyboard file completely unmodified - nothing to
// reapply after a git pull. Every _kb override below calls through to _user.

// ORDER IS LOAD-BEARING. layer_switch_get_layer() ORs default_layer_state into
// the active layers and scans from the HIGHEST index down, so a default layer
// above an overlay makes that overlay unreachable. _HRM is a default layer
// (UC_HRM persists it), and it has no transparent keys - parked at 7 it answered
// every lookup first and killed layers 1-6 outright, with no way back from the
// keyboard because Fn+T resolved to plain T. It must stay below every overlay.
// Measured 2026-08-04. Never reference these as bare numbers.
enum layers {
    _BASE = 0,
    _HRM,   // 1  same base, but with home row mods - opt-in via UC_HRM
    _NAV,   // 2  F-keys, arrows, browser
    _NUM,   // 3  digits and the common symbols
    _MEDIA, // 4  transport, volume, settings; also NAV+NUM held together
    _WM,    // 5  tmux and window management, merged onto one layer
    _SPR1,  // 6  spare - empty, transparent, reachable via Nav/Fn + F22
    _SPR2,  // 7  spare - empty, transparent, reachable via Nav/Fn + F23
};

// DYNAMIC_KEYMAP_LAYER_COUNT is 8 (fs026_eeprom.h), and keymap_introspection.c
// static-asserts that keymaps[] has no more layers than that. Eight is the
// ceiling, not a target - the two spares exist so a new idea has somewhere to
// go without renumbering anything.

// ===========================================================================
// Macro strings
//
// tmux only. The prefix is one #define: change TMUX_PFX if yours is not C-b
// and every sequence below follows.
//
// SS_DELAY after the prefix is deliberate. tmux is reading a pty; back-to-back
// reports can arrive inside one read and the command byte gets eaten, which
// looks exactly like "the key did nothing".
// ===========================================================================

#define TMUX_PFX SS_LCTL("b") SS_DELAY(20)

enum custom_keycodes {
    // tmux: panes
    T_SPLTV = SAFE_RANGE, // prefix %  split side by side
    T_SPLTH,              // prefix "  split stacked
    T_ZOOM,               // prefix z  zoom / unzoom the pane
    T_KILLP,              // prefix x  kill the pane
    T_PANEL,              // prefix <Left>
    T_PANED,              // prefix <Down>
    T_PANEU,              // prefix <Up>
    T_PANER,              // prefix <Right>
    // tmux: windows and sessions
    T_NEWW,               // prefix c  new window
    T_PREVW,              // prefix p
    T_NEXTW,              // prefix n
    T_LASTW,              // prefix l  back to the previous window
    T_LISTW,              // prefix w  choose a window
    T_SESS,               // prefix s  choose a session
    T_DETACH,             // prefix d
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
        case T_SPLTV:  SEND_STRING(TMUX_PFX "%");               return true;
        case T_SPLTH:  SEND_STRING(TMUX_PFX "\"");              return true;
        case T_ZOOM:   SEND_STRING(TMUX_PFX "z");               return true;
        case T_KILLP:  SEND_STRING(TMUX_PFX "x");               return true;
        case T_PANEL:  SEND_STRING(TMUX_PFX SS_TAP(X_LEFT));    return true;
        case T_PANED:  SEND_STRING(TMUX_PFX SS_TAP(X_DOWN));    return true;
        case T_PANEU:  SEND_STRING(TMUX_PFX SS_TAP(X_UP));      return true;
        case T_PANER:  SEND_STRING(TMUX_PFX SS_TAP(X_RIGHT));   return true;

        case T_NEWW:   SEND_STRING(TMUX_PFX "c");               return true;
        case T_PREVW:  SEND_STRING(TMUX_PFX "p");               return true;
        case T_NEXTW:  SEND_STRING(TMUX_PFX "n");               return true;
        case T_LASTW:  SEND_STRING(TMUX_PFX "l");               return true;
        case T_LISTW:  SEND_STRING(TMUX_PFX "w");               return true;
        case T_SESS:   SEND_STRING(TMUX_PFX "s");               return true;
        case T_DETACH: SEND_STRING(TMUX_PFX "d");               return true;
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
    TD_MEDIA_TRANSPORT,
};

#define TD_SFT  TD(TD_SFT_CAPS)
#define TD_MPLY TD(TD_MEDIA_TRANSPORT)

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

// Earbud-style transport on one key: tap = play/pause, double = next, triple =
// previous. A media key has no typing rhythm to disturb, so unlike a letter key
// the decision delay costs nothing.
static void td_media_finished(tap_dance_state_t *state, void *user_data) {
    switch (state->count) {
        case 1:  tap_code(KC_MPLY); break;
        case 2:  tap_code(KC_MNXT); break;
        default: tap_code(KC_MPRV); break;
    }
}

// Left Ctrl used to be a tap dance too, because double-tapping it armed the
// leader. Even with the mod registered on the keydown, that put Ctrl inside the
// tap dance state machine with a 200ms term, and Ctrl+A and friends felt late.
// The leader is gone and Ctrl is a plain KC_LCTL - nothing to resolve, nothing
// to wait for.
tap_dance_action_t tap_dance_actions[] = {
    [TD_SFT_CAPS]        = ACTION_TAP_DANCE_FN_ADVANCED_WITH_RELEASE(td_shift_each_tap, td_shift_each_release, td_shift_finished, td_shift_reset),
    [TD_MEDIA_TRANSPORT] = ACTION_TAP_DANCE_FN(td_media_finished),
};

// The three space bars. Grouped because all four hooks below need the same test.
static inline bool is_thumb_layer_tap(uint16_t keycode) {
    switch (keycode) {
        case LT(_NUM, KC_SPC):
        case LT(_MEDIA, KC_SPC):
        case LT(_NAV, KC_SPC):
            return true;
        default:
            return false;
    }
}

// THE SPACE BARS NEED A LONG TERM. This is not tuning, it is the fix for a board
// that looked broken - measured 2026-08-17.
//
// A tap-hold key resolves as a HOLD the instant it is held past its term, even
// with nothing else pressed. At the old global 130ms an ordinary space bar press
// was already past it (thumbs rest on space; 130-250ms is completely normal), so
// the layer came on silently, the space itself was swallowed - a hold has no tap -
// and the next letter was read off that layer instead of the alphabet:
//
//   left space  (_NUM)    O -> 9      every letter -> a digit or symbol
//   middle      (_MEDIA)  O -> nothing (XXXXXXX)   F -> QK_REP, a doubled
//                         character   A -> SE_LOCK, the board stops typing
//                         S -> SH_TOGG, every key mirrored
//                         T -> UC_HRM, home row mods on, persisted to EEPROM
//   right space (_NAV)    O -> F9, i.e. nothing    B -> QK_REP, doubled character
//                         A -> Home   C -> browser Back   X -> CG_TOGG
//
// That single mechanism produces all three reported symptoms: letters that do not
// appear, characters that double, and words that run together.
//
// 230ms costs no typing latency whatsoever. The tap fires on RELEASE, so a 90ms
// space still lands at 90ms no matter what the term is; the term only decides how
// long a still-held thumb waits before it becomes a layer.
uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    if (is_thumb_layer_tap(keycode)) {
        return THUMB_TAPPING_TERM;
    }
    switch (keycode) {
        case TD_SFT:
            return 200;
        case HM_A: case HM_S: case HM_D: case HM_F:
        case HM_J: case HM_K: case HM_L:
            return 180;
        case LT(_NAV, KC_TAB):
            // Same failure mode, smaller blast radius: Tab held a touch past 130ms
            // put _NAV on and the next letter became an F-key or an arrow. Free for
            // the same reason - the Tab tap fires on release either way, and
            // pre_process_record_kb below owns the Alt+Tab case.
            return 180;
        default:
            return TAPPING_TERM;
    }
}

// The other half of the space fix. Hold a thumb through a thinking pause, release
// it without pressing anything, and you still get your space - instead of losing it
// because the key had already resolved as a hold. Pressing any key during the hold
// clears the priming, so a real layer chord never emits a stray space.
bool get_retro_tapping(uint16_t keycode, keyrecord_t *record) {
    return is_thumb_layer_tap(keycode);
}

// Permissive hold is right for the home row MODS and wrong for the LT() space
// bars. Rolling space into the next letter - space down, letter down, letter up,
// space up - matches permissive hold's rule exactly, so it resolved as a hold
// and the roll produced a digit instead of "space letter". Restricting it to
// mod-taps means a layer tap is decided by the tapping term alone.
//
// That was only half the answer, and the missing half is what kept the bug alive:
// "the tapping term alone" is fine as long as the term is longer than a plain
// press of the key. At 130ms it was not - see get_tapping_term() above.
bool get_permissive_hold(uint16_t keycode, keyrecord_t *record) {
    return IS_QK_MOD_TAP(keycode);
}

// The third half of the space fix, and the one that was missing: how a thumb
// becomes a layer QUICKLY.
//
// Permissive hold is off for the LT() thumbs and hold-on-other-key-press was off
// for everything, which left the 230ms term as the only route into _NUM/_MEDIA/
// _NAV. Reaching a layer therefore meant holding a thumb for a quarter of a
// second before the chord key would even count - and worse, action_tapping.c
// parks every key pressed while a tap-hold is undecided in the waiting buffer,
// so those keystrokes came out late in a burst. Slow layers and "typing feels
// laggy" were one bug wearing two hats.
//
// So: once the thumb has been down THUMB_HOLD_ARM_TIME, the next keydown settles
// it as a hold immediately. record is the tap-hold key's own record (see
// TAP_GET_HOLD_ON_OTHER_KEY_PRESS in action_tapping.c), so event.time is when the
// thumb went down and timer_elapsed on it is how long it has been held.
//
// The arm window is the whole reason this is safe. Applying the rule from 0ms is
// bare HOLD_ON_OTHER_KEY_PRESS, which is a strictly more aggressive version of
// the permissive hold that produced digits instead of spaces. Flow Tap already
// covers the in-flow roll by settling the space as a tap on its keydown; the arm
// window covers the roll that starts after a pause, where Flow Tap is out.
bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    if (is_thumb_layer_tap(keycode) || keycode == LT(_NAV, KC_TAB)) {
        return timer_elapsed(record->event.time) >= THUMB_HOLD_ARM_TIME;
    }
    return false; // mod-taps keep permissive hold; nothing else is a tap-hold
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
//   - as the key being pressed, a layer-tap gets a SHORTER window than a mod-tap,
//     not none at all. "None at all" was the original reading and it left the
//     space bar landing on the finger lift for every word; the short window keeps
//     the space instant without stranding the thumb layers;
//   - as the PREVIOUS key, a layer-tap absolutely must count as typing -
//     LT(_NUM,KC_SPC) is the space bar, so excluding it broke the flow chain after
//     every single space, and the first letter of every word went back to
//     resolving on release.
//
// get_flow_tap_term() takes precedence over is_flow_tap_key(), so this is the
// only hook needed.

// Does this key mean "the user is mid-flow"? Judged on the tap keycode, so the
// space bar counts even though it is really LT(_NUM,KC_SPC).
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
    if (!IS_QK_MOD_TAP(keycode) && !IS_QK_LAYER_TAP(keycode)) {
        return 0;
    }
    if ((get_mods() & (MOD_MASK_CG | MOD_BIT_LALT)) != 0) {
        return 0; // mid-hotkey, leave the hold alone
    }
    if (!flow_prev_is_typing(prev_keycode)) {
        return 0;
    }
    if (IS_QK_LAYER_TAP(keycode)) {
        // This used to be a blanket `return 0` reasoned as "a layer hold must
        // always be reachable", and that is what made every space in every
        // sentence land on the finger LIFT rather than the press. Flow Tap settles
        // a tap-hold key as a tap on the KEYDOWN, so inside this window the space
        // is instant and cannot become a layer at all.
        //
        // 110ms rather than the mods' 200ms so that "type a word, then hold the
        // thumb for digits" still reaches _NUM - that reach always has a pause in
        // front of it, a roll never does. LT(_NAV,KC_TAB) stays out: Alt+Tab is
        // already handled in pre_process_record_kb and Tab-as-Nav is deliberate.
        return is_thumb_layer_tap(keycode) ? FLOW_TAP_TERM_THUMB : 0;
    }
    return FLOW_TAP_TERM;
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

// Priority order matters as much as the colours: permission (90) > done (85) >
// error (80) > working/loading (20) > idle (10). done outranks error so that
// finishing a turn always lands on green, even if something failed along the
// way - otherwise a stale red sits on top of the green finish for its whole TTL
// and you never see it.
//
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
        case CLAUDE_DONE:       bus_set(BUS_SLOT_CLAUDE, PAT_STROBE3, 0, 255,   0, 85, 2200); break;
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
//
// Cost, stated once so it is not rediscovered: a key that belongs to ANY combo
// has its keydown withheld by process_combo() until the combo is ruled out - by
// the next keydown, by the release, or by COMBO_TERM. These nine cover
// Q W Z X C V N M , . ' J K P. Picking rare digraphs prevents false triggers, it
// does not remove the delay; that is inherent to combos. Kept because they are
// wanted. Lower COMBO_TERM to trade recognition slack for less of it.
const uint16_t PROGMEM combo_esc[]   = {KC_Q, KC_W, COMBO_END};
const uint16_t PROGMEM combo_undo[]  = {KC_Z, KC_X, COMBO_END};
const uint16_t PROGMEM combo_caps[]  = {KC_C, KC_V, COMBO_END};
const uint16_t PROGMEM combo_del[]   = {KC_N, KC_M, COMBO_END};
const uint16_t PROGMEM combo_mins[]  = {KC_M, KC_COMM, COMBO_END};
const uint16_t PROGMEM combo_unds[]  = {KC_COMM, KC_DOT, COMBO_END};
const uint16_t PROGMEM combo_lock[]  = {KC_Q, KC_P, COMBO_END}; // opposite corners, two hands
// jk is the vim escape; the digraph is absent from English. Defined on the
// plain keycodes only, so on _HRM (HM_J/HM_K) it simply doesn't exist and the
// "no combos on home-row-mod keys" rule holds.
const uint16_t PROGMEM combo_vimesc[] = {KC_J, KC_K, COMBO_END};
// Colon otherwise needs layer 2 plus shift; ".'" never occurs in prose.
const uint16_t PROGMEM combo_coln[]   = {KC_DOT, KC_QUOT, COMBO_END};

combo_t key_combos[] = {
    COMBO(combo_esc,    KC_ESC),
    COMBO(combo_undo,   LCTL(KC_Z)),
    COMBO(combo_caps,   CW_TOGG),
    COMBO(combo_del,    KC_DEL),
    COMBO(combo_mins,   KC_MINS),
    COMBO(combo_unds,   KC_UNDS),
    COMBO(combo_lock,   QK_SECURE_LOCK),
    COMBO(combo_vimesc, KC_ESC),
    COMBO(combo_coln,   KC_COLN),
};

// ===========================================================================
// Key overrides, tri layer
// ===========================================================================

// Shift+Backspace = Delete, mods suppressed. Overrides match the literal
// keymap keycode, so the LT() space bars can't carry one - which is why there
// is no shift+space=underscore here (the ,+. combo covers _ instead).
#ifdef KEY_OVERRIDE_ENABLE
const key_override_t shift_bspc_del = ko_make_basic(MOD_MASK_SHIFT, KC_BSPC, KC_DEL);

const key_override_t *key_overrides[] = {&shift_bspc_del};
#endif

// Hold both outer space bars (Nav + Num) together to get _MEDIA, so the system
// layer is reachable without moving a thumb to the middle Fn key.
//
// Deliberately NOT update_tri_layer_state(). That helper CLEARS layer3 on every
// layer change where layer1+layer2 are not both held:
//     return (state & mask12) == mask12 ? (state | mask3) : (state & ~mask3);
// which silently killed the Fn key. LT(_MEDIA,KC_SPC) set bit 3, this callback ran
// and stripped it again, so holding Fn reached nothing and every documented
// "Fn + X" was unreachable - the tri-layer chord was the only way in.
//
// So: only ever ADD _MEDIA here, and only take it back off if the tri-layer is
// what put it there. `tri_owns_media` is that ownership bit.
static bool tri_owns_media = false;

layer_state_t layer_state_set_kb(layer_state_t state) {
    const layer_state_t tri   = ((layer_state_t)1 << _NAV) | ((layer_state_t)1 << _NUM);
    const layer_state_t media = (layer_state_t)1 << _MEDIA;

    if ((state & tri) == tri) {
        if (!(state & media)) {
            tri_owns_media = true; // Fn was not already holding it
        }
        state |= media;
    } else if (tri_owns_media) {
        tri_owns_media = false;
        state &= ~media;
    }
    return layer_state_set_user(state);
}

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
    // Auto Shift boots enabled in QMK; apply the persisted opt-in before the
    // first keystroke. autoshift_disable() is RAM-only - NOTHING in this
    // function may write EEPROM. autocorrect_enable()/disable() both do
    // (eeconfig_update_keymap); keep such calls out of init.
#ifdef AUTO_SHIFT_ENABLE
    if (!user_config.autoshift_on) autoshift_disable();
#endif
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

// Tab is LT(_NAV,KC_TAB) again, and this hook is the only reason that is safe.
//
// A tap-hold key emits its tap on RELEASE, so plain LT() on Tab meant Alt+Tab
// fired nothing until the finger came up - and holding Tab past the tapping term
// gave the Nav layer and no Tab at all. That is not fixable from
// process_record_kb: for a tap-hold key, action_tapping.c buffers the keydown and
// only calls process_record() once it has DECIDED, so anything there runs too
// late by construction.
//
// pre_process_record_kb runs in action_exec() at action.c:133, before
// action_tapping_process(). Returning false here skips the tap-hold machinery for
// this event entirely. So: if any modifier is already down, the user is typing
// Alt+Tab / Ctrl+Tab / Shift+Tab, never reaching for a layer - send a real Tab on
// the keydown and hold it, exactly as a plain Tab key would (so holding it
// repeats and walks the window list).
//
// The trade-off is deliberate: Nav cannot be reached by Tab while a modifier is
// held. Space R is the other Nav key and is unaffected.
static bool tab_mod_bypass = false;

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (keycode == LT(_NAV, KC_TAB) && !secure_is_locked()) {
        uint8_t mods = get_mods();
#ifndef NO_ACTION_ONESHOT
        mods |= get_oneshot_mods();
#endif
        if (record->event.pressed) {
            if (mods) {
                tab_mod_bypass = true;
                register_code(KC_TAB);
                return false;
            }
        } else if (tab_mod_bypass) {
            tab_mod_bypass = false;
            unregister_code(KC_TAB);
            return false;
        }
    }
    return pre_process_record_user(keycode, record);
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
    // Base. Plain letters - home row mods live on _HRM (layer 1) instead.
    //
    // Ctrl is a plain KC_LCTL (it was a tap dance, which is what made Ctrl+A feel
    // late). Tab keeps its Nav hold, but only because pre_process_record_kb above
    // sends a real Tab the instant any modifier is down - without that hook,
    // Alt+Tab does not work on an LT() Tab key.
    [_BASE] = LAYOUT_tkl_ansi(
        QK_GESC     , KC_Q        , KC_W        , KC_E        , KC_R        , KC_T        , KC_Y        , KC_U        , KC_I        , KC_O        , KC_P        , KC_BSPC,
        LT(_NAV,KC_TAB), KC_A     , KC_S        , KC_D        , KC_F        , KC_G        , KC_H        , KC_J        , KC_K        , KC_L        , KC_ENT,
        TD_SFT      , OSL(_NUM)      , KC_Z        , KC_X        , KC_C        , KC_V        , KC_B        , KC_N        , KC_M        , KC_COMM     , KC_DOT      , KC_QUOT,
        KC_LCTL     , KC_LGUI     , KC_LALT     , LT(_NUM,KC_SPC), LT(_MEDIA,KC_SPC), LT(_NAV,KC_SPC), KC_F21      , KC_F22      , KC_F23
    ),
    // Identical to _BASE but with home row mods on ASDF / JKL. UC_HRM swaps the
    // default layer here and persists it, so they are opt-in without reflashing.
    // Kept SECOND in this array on purpose: qmk c2json --no-cpp indexes layers by
    // their position in the source, not by the [_NAME] designator, so the drawing
    // mislabels every layer if source order and enum order disagree.
    [_HRM] = LAYOUT_tkl_ansi(
        QK_GESC     , KC_Q        , KC_W        , KC_E        , KC_R        , KC_T        , KC_Y        , KC_U        , KC_I        , KC_O        , KC_P        , KC_BSPC,
        LT(_NAV,KC_TAB), HM_A     , HM_S        , HM_D        , HM_F        , KC_G        , KC_H        , HM_J        , HM_K        , HM_L        , KC_ENT,
        TD_SFT      , OSL(_NUM)      , KC_Z        , KC_X        , KC_C        , KC_V        , KC_B        , KC_N        , KC_M        , KC_COMM     , KC_DOT      , KC_QUOT,
        KC_LCTL     , KC_LGUI     , KC_LALT     , LT(_NUM,KC_SPC), LT(_MEDIA,KC_SPC), LT(_NAV,KC_SPC), KC_F21      , KC_F22      , KC_F23
    ),
    // Nav / F-keys. Bottom-right cluster reaches _WM and the two spares
    // momentarily; the same three keys on _MEDIA latch them instead.
    [_NAV] = LAYOUT_tkl_ansi(
        QK_GESC     , KC_F1       , KC_F2       , KC_F3       , KC_F4       , KC_F5       , KC_F6       , KC_F7       , KC_F8       , KC_F9       , KC_F10      , KC_DEL,
        _______     , KC_HOME     , KC_PGDN     , KC_PGUP     , KC_END      , KC_INS      , KC_LEFT     , KC_DOWN     , KC_UP       , KC_RIGHT    , KC_ENT,
        TD_SFT      , _______     , KC_PSCR     , CG_TOGG     , KC_WBAK     , KC_WFWD     , QK_REP      , QK_AREP     , XXXXXXX     , KC_F11      , KC_F12      , QK_LLCK,
        _______     , _______     , _______     , _______     , _______     , LT(_NUM,KC_SPC), MO(_WM)     , MO(_SPR1)   , MO(_SPR2)
    ),
    // Digits and everyday symbols. Shift (the key that is TD_SFT here) reaches the
    // shifted half of every one of them, so this layer covers the whole ASCII set.
    //
    // The M position is KC_BSLS, not KC_NUBS. KC_NUBS is the *ISO* extra key; the
    // `us` xkb layout leaves it unmapped, so it typed nothing at all - while
    // keymap-drawer cheerfully rendered it as "\ |" and the drawing lied about it.
    [_NUM] = LAYOUT_tkl_ansi(
        QK_GESC   , KC_1      , KC_2      , KC_3      , KC_4      , KC_5      , KC_6      , KC_7      , KC_8      , KC_9      , KC_0      , KC_BSPC,
        _______   , KC_MINS   , KC_EQL    , KC_SCLN   , KC_QUOT   , KC_GRV    , KC_LBRC   , KC_RBRC   , KC_SLSH   , S(KC_SLSH), KC_ENT,
        TD_SFT    , _______   , _______   , _______   , _______   , _______   , _______   , _______   , KC_BSLS   , KC_COMM   , KC_DOT    , QK_LLCK,
        _______   , _______   , _______   , _______   , _______   , KC_SPC    , _______   , _______   , _______
    ),
    // Media + system. Settings on the top row persist to EEPROM; the home row
    // holds lock / one-handed / Caps Word next to the transport keys, and the
    // bottom row is one-shot mods.
    //
    // The three Claude keys are the layer-travel keys here: TG() latches, so you
    // let go of Fn and stay on WM / Spare 1 / Spare 2 with both hands free. The
    // Nav route (hold Space R + the same key) is momentary and can only reach the
    // half of those layers your free hand can still get to - which is why the
    // latching route exists. Fn + the same key again comes back: the bottom row
    // of those layers is transparent, so Fn always reaches this layer.
    //
    // Fn + Gui is the panic key - TO(_BASE) drops every latched layer at once.
    // It sits on the bottom row for the same reason: that row is the only region
    // transparent on all three travel layers, so this escape works from any of
    // them. A latched _WM masks the whole alphabet with Super chords and reads as
    // a dead keyboard, so there has to be one key that always gets you home.
    [_MEDIA] = LAYOUT_tkl_ansi(
        _______, UC_LOCKB     , UC_CLEDS     , UC_BRTD      , UC_BRTU      , UC_HRM , UC_AURA, UC_RAIN, RM_TOGG  , XXXXXXX, XXXXXXX , _______,
        _______, SE_LOCK      , SH_TOGG      , CW_TOGG      , QK_REP       , KC_MPRV, TD_MPLY, KC_MNXT, KC_VOLD  , KC_VOLU, _______,
        _______, OSM(MOD_LSFT), OSM(MOD_LCTL), OSM(MOD_LALT), OSM(MOD_LGUI), QK_LOCK, DM_REC1, DM_PLY1, DM_REC2  , DM_PLY2, DM_RSTP , QK_LLCK,
        _______, TO(_BASE)    , _______      , _______      , _______      , _______, TG(_WM), TG(_SPR1), TG(_SPR2)
    ),
    // Tmux + windows, merged onto one layer. Left hand drives tmux, right hand
    // drives the window manager, and the split is the same on both rows:
    //
    //   row 1  A S D F G  tmux panes          H J K L  tmux pane focus (arrows)
    //   row 2  Z X C V B  tmux windows        N M , .  Hyprland window focus
    //          + the key left of Z: last window     + ': close window
    //   row 0  1 - 0                          Hyprland workspaces 1-10
    //
    // Workspaces earn a whole row here because on a 40% they are otherwise
    // unreachable: Super+3 on the base layer means holding Gui AND the Num
    // thumb AND E, which is not a chord anyone hits twice.
    //
    // Everything tmux is a macro sending the real prefix sequence, so tmux needs
    // no config; everything WM is a plain Super chord, which is what omarchy
    // binds out of the box.
    [_WM] = LAYOUT_tkl_ansi(
        _______, LGUI(KC_1), LGUI(KC_2), LGUI(KC_3), LGUI(KC_4), LGUI(KC_5), LGUI(KC_6)   , LGUI(KC_7)   , LGUI(KC_8) , LGUI(KC_9)    , LGUI(KC_0), _______,
        _______, T_NEWW    , T_SPLTV   , T_SPLTH   , T_ZOOM    , T_KILLP   , T_PANEL      , T_PANED      , T_PANEU    , T_PANER       , _______,
        _______, T_LASTW   , T_PREVW   , T_NEXTW   , T_LISTW   , T_SESS    , T_DETACH     , LGUI(KC_LEFT), LGUI(KC_DOWN), LGUI(KC_UP) , LGUI(KC_RIGHT), LGUI(KC_W),
        _______, TO(_BASE) , _______   , _______   , _______   , _______   , _______      , _______      , _______
    ),
    // Two spares. Fully transparent, so reaching one changes nothing until you
    // put something on it - in VIA (layers 6 and 7) or here. They exist so the
    // eight layers the EEPROM allocates are all accounted for and nothing has to
    // be renumbered later; layer indices are load-bearing on this board.
    [_SPR1] = LAYOUT_tkl_ansi(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______
    ),
    [_SPR2] = LAYOUT_tkl_ansi(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______
    )

};
// clang-format on
