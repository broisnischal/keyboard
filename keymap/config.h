/* Copyright 2025 Carlos Eduardo de Paula <carlosedp@gmail.com>
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

// ---------------------------------------------------------------------------
// Matrix / latency
//
// The board already ships the right debounce algorithm: asym_eager_defer_pk
// registers a press on the FIRST edge (zero added press latency) and only
// defers the release, which is what actually suppresses chatter. Lowering
// DEBOUNCE would not speed up typing - it would only shorten the release-side
// guard. Left at 7 deliberately.
// ---------------------------------------------------------------------------
#undef DEBOUNCE
#define DEBOUNCE 7

// Counts matrix scans/second. Readable over raw HID (see CLAUDE_SUB_SCANRATE),
// so we get a real number without pulling in a CONSOLE USB interface.
#define DEBUG_MATRIX_SCAN_RATE

// ---------------------------------------------------------------------------
// Tap-hold / home row mods
//
// 130ms globally keeps the LT() keys feeling exactly as before; the home row
// mods and the shift tap dance get longer terms via get_tapping_term().
// ---------------------------------------------------------------------------
#define TAPPING_TERM 130
#define TAPPING_TERM_PER_KEY

// Same-hand chords settle as taps, so rolling "df" types df instead of firing
// Ctrl. Handedness comes from chordal_hold_layout in keymap.c.
#define CHORDAL_HOLD

// A tap-hold key held while another key is pressed AND released resolves as
// hold. With CHORDAL_HOLD guarding same-hand rolls, this is safe and makes
// opposite-hand mods engage without waiting out the tapping term.
#define PERMISSIVE_HOLD

// No hold-to-repeat on the mod keys: holding A after tapping it gives GUI, not
// "aaaa". Key repeat lives on QK_REP instead.
#define QUICK_TAP_TERM 0

// While you are actually typing, holds are disabled outright - so mid-word
// there is no home-row-mod latency at all. Only applies once you pause.
//
// 200ms not 150: this is the maximum gap between keystrokes that still counts
// as "typing". At 150 an ordinary uneven rhythm kept dropping out of flow, and
// every dropout is a keystroke that resolves on release instead of on press -
// which is exactly the drag that gets described as "subtly slow". The cost is
// that you must pause 200ms before a home row mod engages, which is roughly
// what deliberately reaching for a modifier takes anyway.
#define FLOW_TAP_TERM 200

// ---------------------------------------------------------------------------
// Combos
//
// Short term so a fast roll of two letters types the letters rather than
// firing the combo. The chosen pairs are also digraphs that barely occur in
// English ("qw", "zx", "cv", "nm"), which matters more than the term does.
// ---------------------------------------------------------------------------
#define COMBO_TERM 40

// ---------------------------------------------------------------------------
// One-shot keys
// ---------------------------------------------------------------------------
#define ONESHOT_TIMEOUT 3000    // a pending one-shot mod gives up after 3s
#define ONESHOT_TAP_TOGGLE 2    // tap a one-shot mod twice to lock it on

// ---------------------------------------------------------------------------
// Caps Word
// ---------------------------------------------------------------------------
#define CAPS_WORD_IDLE_TIMEOUT 5000

// ---------------------------------------------------------------------------
// Secure: pattern-to-unlock
//
// SAFETY: QMK's Secure boots SECURE_LOCKED and, by default, re-locks after 60s
// idle. Wired up naively that is a keyboard that is dead on arrival and locks
// itself while you read. So:
//   - idle auto-lock is OFF; locking is only ever explicit
//   - the firmware unlocks at boot UNLESS the EEPROM lock_on_boot flag is set,
//     and that flag ships off (see user_config_t in keymap.c)
//
// Unlock pattern is matrix positions, so it is layer-independent:
//   N (3,7) -> E (1,3) -> E (1,3) -> S (2,2)
// CHANGE THIS. It is the one setting in here worth making your own.
// ---------------------------------------------------------------------------
#define SECURE_IDLE_TIMEOUT 0
#define SECURE_UNLOCK_TIMEOUT 5000
#define SECURE_UNLOCK_SEQUENCE {{3, 7}, {1, 3}, {1, 3}, {2, 2}}

// ---------------------------------------------------------------------------
// Persistent user settings (see user_config_t in keymap.c)
// ---------------------------------------------------------------------------
#define EECONFIG_USER_DATA_SIZE 4

// A fresh EEPROM (every reflash invalidates VIA's magic) was landing on a stock
// animation, so the board stopped mirroring Claude after each flash and had to
// be re-selected by hand. Make the Claude effect the out-of-box default.
#undef RGB_MATRIX_DEFAULT_MODE
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CUSTOM_CLAUDE_AURA
