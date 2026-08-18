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
// 130ms is the floor for plain keys. The home row mods, the shift tap dance and
// - critically - the three LT(n,KC_SPC) thumbs get longer terms via
// get_tapping_term(); see the table above it for what 130ms on a space bar did.
// ---------------------------------------------------------------------------
#define TAPPING_TERM 130
#define TAPPING_TERM_PER_KEY

// Held past the term, then released with no other key pressed in between? Send
// the tap anyway. Without this a long "thinking pause" space produced NO space at
// all - the key resolved as a hold, and a hold has no tap - so two words silently
// ran together. Restricted to the space thumbs by get_retro_tapping(); Nav is
// routinely entered on Tab and then abandoned, and a stray Tab is worse than a
// missing one.
#define RETRO_TAPPING_PER_KEY

// Same-hand chords settle as taps, so rolling "df" types df instead of firing
// Ctrl. Handedness comes from chordal_hold_layout in keymap.c.
#define CHORDAL_HOLD

// A tap-hold key held while another key is pressed AND released resolves as
// hold. Correct for the home row MODS - an opposite-hand chord engages without
// waiting out the tapping term.
//
// WRONG for the LT() space bars, which is why this is per-key now. Rolling
// "e" + space + "a" fast means space goes down, 'a' goes down and up, space
// comes up - permissive hold called that a HOLD, so the roll silently typed a
// digit instead of "space a". get_permissive_hold() in keymap.c restricts it to
// mod-taps; layer taps decide on the tapping term alone.
//
// Which only works if that term is longer than a plain press of the key, and at
// the global 130ms it was not - an ordinary space bar press sailed past it and
// turned its layer on by itself. THUMB_TAPPING_TERM below is the other half.
#define PERMISSIVE_HOLD_PER_KEY

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

// Flow Tap for the space thumbs, which the old get_flow_tap_term() refused
// outright ("a layer hold must always be reachable"). That refusal is why every
// space in every sentence landed on the finger LIFT instead of the press: a
// tap-hold key emits its tap on release, and nothing was settling the space bar
// early. Inside this window the space is settled as a tap on the KEYDOWN, so it
// is instant and the layer cannot engage by accident.
//
// Much shorter than the 200ms the mods get, on purpose. 110ms covers a genuine
// roll - space and the next letter overlapping - while still leaving the thumb
// layers reachable mid-sentence, because deliberately reaching for one always has
// a beat of thought in front of it. Raise it if a fast space still turns a layer
// on; lower it if "word then digits" stops reaching _NUM.
#define FLOW_TAP_TERM_THUMB 110

// A space bar tap is 80-250ms. A thumb deliberately held down for a layer is held
// far longer than that, and the tap still fires on release, so this costs zero
// typing latency - it only lengthens how long a STILL-HELD thumb waits before it
// becomes a layer. At the old global 130ms an ordinary space was already past it.
//
// This term is now only the STANDALONE path: a thumb held with nothing else
// pressed. A thumb held and then chorded with another key resolves on that key's
// press, after THUMB_HOLD_ARM_TIME below.
#define THUMB_TAPPING_TERM 230

// Resolve a thumb (or Tab) as a HOLD the moment another key goes down - but only
// once it has already been down this long.
//
// Without this the LT() thumbs had exactly one way to become a layer: outlast the
// 230ms term. So reaching _NUM meant pressing the thumb, waiting out a quarter of
// a second, and only then pressing the key - and every key pressed inside that
// window sat in the waiting buffer until the thumb resolved, which is the drag
// that reads as general lag. Both are the same missing rule.
//
// The arm time is what keeps it from re-breaking the roll. Bare
// HOLD_ON_OTHER_KEY_PRESS turns "space then letter" into a layer chord, and that
// is the digit-instead-of-space bug all over again. Two guards stop it:
//
//   - Flow Tap already settles the space as a tap on the keydown whenever the
//     previous keystroke was within FLOW_TAP_TERM_THUMB, so mid-sentence the
//     thumb cannot become a hold at all and this rule never runs;
//   - after a pause, where Flow Tap is out, 80ms is the gate. A roll into the
//     next letter overlaps the thumb almost immediately; deliberately holding a
//     thumb for a layer does not. 80ms is well under the 230ms it replaces and
//     still sits below the fastest realistic space-to-letter overlap.
//
// Raise it if a leading space after a thinking pause starts producing digits;
// lower it if layer entry still feels like it needs a deliberate wait.
#define HOLD_ON_OTHER_KEY_PRESS_PER_KEY
#define THUMB_HOLD_ARM_TIME 80

// ---------------------------------------------------------------------------
// Combos
//
// Short term so a fast roll of two letters types the letters rather than
// firing the combo. The chosen pairs are also digraphs that barely occur in
// English ("qw", "zx", "cv", "nm"), which matters more than the term does.
//
// This term is also the latency knob. A key belonging to any combo has its
// keydown withheld until the combo is ruled out, and COMBO_TERM is the worst
// case - so lowering it makes the member keys feel quicker at the cost of
// needing the two presses closer together. 40 is the original tuning.
// ---------------------------------------------------------------------------
#define COMBO_TERM 40

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// One-shot keys
// ---------------------------------------------------------------------------
// 1200ms, not 3000. OSL(_NUM) sits between left Shift and Z, and a one-shot layer
// re-points the NEXT keystroke with a single tap - no hold needed. At 3s a stray
// brush of that key while reaching for Shift meant the next letter came off _NUM
// up to three seconds later ("o" -> "9"), which is indistinguishable from the key
// being broken. A deliberate OSL-then-key is ~300ms, so 1200 loses nothing.
#define ONESHOT_TIMEOUT 1200    // a pending one-shot mod/layer gives up after 1.2s
#define ONESHOT_TAP_TOGGLE 2    // tap a one-shot mod twice to lock it on

// ---------------------------------------------------------------------------
// Caps Word
// ---------------------------------------------------------------------------
#define CAPS_WORD_IDLE_TIMEOUT 5000

// ---------------------------------------------------------------------------
// Auto Shift - hold a key slightly longer for its shifted value
//
// Compiled in but DISABLED at boot unless the persisted flag says otherwise
// (user_config_t in keymap.c): when on, every key must wait out this timeout
// before it can decide, which is exactly the release-latency this board was
// tuned to avoid. Toggle with AS_TOGG (the System layer); the choice persists.
// ---------------------------------------------------------------------------
#define AUTO_SHIFT_TIMEOUT 170

// ---------------------------------------------------------------------------
// Autocorrect - fixes common typos as you type. Zero latency (it edits after
// the fact) and on by default (QMK's own eeconfig default). AC_TOGG (the System layer)
// toggles it and QMK persists that natively in keymap_config - do NOT add
// keymap-side persistence: autocorrect_enable()/disable() write EEPROM, and
// an EEPROM write inside keyboard_post_init_kb hangs boot on this board.
// Custom dictionary: generate autocorrect_data.h into this directory with
// `qmk generate-autocorrect-data`.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Dynamic macros
//
// 32 shared entries instead of the default 128. RAM is 16 KB and this port's
// heap is simply "whatever the linker has left" (data+bss always reads
// ~16,380 - that is padding, not overflow), so a smaller buffer buys back
// ~1 KB of heap headroom. 32 keypresses covers the "record a repetitive
// edit" use case.
// ---------------------------------------------------------------------------
#define DYNAMIC_MACRO_SIZE 32

// ---------------------------------------------------------------------------
// Leader key: REMOVED (LEADER_ENABLE = no).
//
// It was armed by double-tapping Left Ctrl, which meant Ctrl had to be a tap
// dance with a 200ms term - and that is what made Ctrl+A and every other Ctrl
// chord feel late. Ctrl is a plain KC_LCTL again. The tmux prefix sequences the
// leader used to send now live on the Tmux/Window layer as real keys.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Layer Lock - QK_LLCK on each momentary layer makes it stick. The timeout is
// the safety net: a locked layer someone forgot about looks exactly like a
// broken keyboard, so it releases itself after a minute of no typing.
// ---------------------------------------------------------------------------
#define LAYER_LOCK_IDLE_TIMEOUT 60000

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
//
// The keyboard's own config.h pins this at 4 and redefining it differently is
// an error, so the struct packs its booleans as bitfields to stay inside.
// ---------------------------------------------------------------------------
#define EECONFIG_USER_DATA_SIZE 4

// A fresh EEPROM (every reflash invalidates VIA's magic) was landing on a stock
// animation, so the board stopped mirroring Claude after each flash and had to
// be re-selected by hand. Make the Claude effect the out-of-box default.
#undef RGB_MATRIX_DEFAULT_MODE
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CUSTOM_CLAUDE_AURA
