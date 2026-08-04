# Epomaker TH40 - QMK setup & flashing runbook

Everything needed to rebuild and reflash this keyboard from scratch.
Last flashed: **2026-08-03**.

---

## 1. What this keyboard actually is

| | |
|---|---|
| Model | Epomaker TH40, 40% (44 keys), tri-mode (USB / BLE ×3 / 2.4 GHz) |
| USB ID | `36b0:304e` |
| MCU | **es32fs026** (Essemi ES32 / FS026, Cortex-M0, 128 KB flash, 16 KB RAM) |
| Firmware base | Real QMK on ChibiOS + a proprietary wireless blob (`lib/rdmctmzt_common`) |
| Bootloader | LUFA mass-storage, enumerates as `03eb:2045` "RDMCTMZT DFU" |

The `36b0:304e` ID is the marker for the **QMK version (a.k.a. V2)** of the TH40. Epomaker also
shipped VIA-only units; those cannot do any of this. If `lsusb` ever shows a different ID, none of
this document applies.

Stock firmware is VIA-capable but **not** QMK-capable in the ways that matter: no Tap Dance, no
Combos, no Caps Word. Those are compile-time features, so getting them means building and flashing.

---

## 2. Firmware source

Upstream QMK does **not** support this board - the ES32/FS026 port lives only in a community fork:

```
https://github.com/carlosedp/qmk_firmware        # master; TH40 is on its "tested" list
```

Do not use `carlosedp/qmk_firmware_th40` - that's the older, abandoned repo; its own README points
at the one above.

| | |
|---|---|
| Local clone | `/home/nees/key/qmk` |
| Pinned at | `3117f91a0420b2aa56fd46d5a224735edd937a44` (2026-07-31) |
| Keyboard dir | `keyboards/epomaker/th40/` |
| Our keymap | `keyboards/epomaker/th40/keymaps/tapdance/` |

Fresh clone (only the two ARM submodules are needed - a full `git submodule update` pulls ~4 GB of
things this board never touches):

```bash
git clone --depth 1 https://github.com/carlosedp/qmk_firmware.git /home/nees/key/qmk
cd /home/nees/key/qmk
git submodule update --init --recursive --depth 1 lib/chibios lib/chibios-contrib
```

Toolchain (already installed on this machine): `qmk`, `arm-none-eabi-gcc`, `make`, `dfu-util`.

---

## 3. The keymap

Source of truth for the layout is the VIA export, backed up here:

```
/home/nees/key/th40-via-keymap-backup.json
```

It was decoded into `keymaps/tapdance/keymap.c` programmatically, not by hand. Two details that
matter if you ever redo that conversion:

- A VIA export is a **raw 5×12 matrix, row-major** (60 entries/layer, `KC_NO` for the 16 unused
  positions). `LAYOUT_tkl_ansi` takes the 44 real ones in this order:
  `(0,0)`, `(1,1..11)`, `(2,0..10)`, `(3,0..11)`, `(4,0)`, `(4,2..4)`, `(4,6..10)`.
- VIA writes legacy keycode names. These needed translating: `KC_GESC`→`QK_GESC`,
  `MAGIC_TOGGLE_CTL_GUI`→`CG_TOGG`. (`KC_WWW_BACK`/`KC_WWW_FORWARD` still resolve but
  `KC_WBAK`/`KC_WFWD` are the current spellings.)

### Layer map

| Layer | Reached by | Contents |
|---|---|---|
| 0 `_BASE` | - | alphas, home-row mods, three Claude keys |
| 1 `_NAV` | hold Tab, or `LT(1,SPC)` | F1-F12, arrows, Home/End/PgUp/PgDn, browser back/fwd, `QK_REP`/`QK_AREP` |
| 2 `_NUM` | `OSL(2)`, or `LT(2,SPC)` | digits, everyday symbols |
| 3 `_MEDIA` | `LT(3,SPC)`, or both outer spaces together (tri layer) | transport, volume, **and the system/settings keys** |
| 4 `_CODE` | `MO()` on **layer 1 + bottom-right key 1**, or `TG()` on **layer 3 + the same key** | operators, digraphs, paired delimiters |
| 5 `_WM` | `MO()` on **layer 1 + bottom-right key 2**, or `TG()` on **layer 3 + the same key** | Hyprland workspaces and windows |
| 6 `_GIT` | `MO()` on **layer 1 + bottom-right key 3**, or `TG()` on **layer 3 + the same key** | git / shell macros |

The bottom-right cluster is the Claude keys on layer 0 and the layer-reach keys on
layer 1, so everything "extra" lives under one thumb-adjacent group. Eight dynamic-keymap
layers exist (`lib/rdmctmzt_common/fs026_eeprom.h`), so there is one spare.

The `TG()` route on layer 3 exists because the `MO()` route is only half usable: it costs two
held fingers, and `_GIT`'s right-hand half (`git log`, `git diff`, `git checkout`, `git branch`,
`git stash`) is then unreachable. Latching frees both hands. It is placed on layer 3's bottom row
because **the bottom row is the only region transparent on all three of `_CODE`/`_WM`/`_GIT`** -
so `LT(3,SPC)` still reaches layer 3 from a latched layer, and the same `Fn` + key press turns it
back off. `TO(_BASE)` sits next to it on the Gui position as the panic key: a latched `_WM` masks
the whole alphabet with `LGUI()` chords and is indistinguishable from a dead board, so one key has
to drop every latched layer at once. Nothing about this persists - `layer_state` is RAM.

**Layer 4 - code.** Number row is `! @ # $ % ^ & * - +`. Home row is the digraphs you
actually type: `-> => != == && ||` then `() [] {}` which insert both delimiters and put the
cursor between them. Bottom row is `~ \` \ | < >` plus `<= >= ::` and `""`.

**Layer 5 - Hyprland.** Sends real chords (`SUPER+1..0`, `SUPER+SHIFT+1..9`, `SUPER+arrows`,
`SUPER+W/F/J/P/C/V`) that omarchy's own bindings already catch, so there is no WM config to
maintain. Workspace binds use `code:10..19`, i.e. physical keycodes, so the shifted variants
work correctly.

**Layer 6 - git/shell.** `SEND_STRING` macros. **These strings are guesses at the workflow** -
they live in one `send_macro()` table at the top of `keymap.c`; changing a command is a
one-line edit.

### Layer 3 - media + system

The old media layer had three empty rows, so the settings keys live there:

| Row | Keys |
|---|---|
| top | `UC_LOCKB` (toggle lock-on-boot) · `UC_CLEDS` (toggle Claude LEDs) · `UC_BRTD` / `UC_BRTU` (LED brightness) |
| home | `SE_LOCK` · `SH_TOGG` (one-handed) · `CW_TOGG` (Caps Word) · `QK_REP` · then prev/play/next/vol− /vol+ (play is a tap dance: 1=play 2=next 3=prev) |
| bottom | one-shot `Shift` · `Ctrl` · `Alt` · `GUI` · `QK_LOCK` (key lock) · `QK_LLCK` on the quote key |
| thumb | `TO(_BASE)` on Gui (panic) · `TG(_CODE)` / `TG(_WM)` / `TG(_GIT)` on the three Claude keys |

### Persistent settings (EEPROM)

Four bytes of `EECONFIG_USER_DATA` hold `lock_on_boot`, `claude_leds`, `led_brightness`. The four
top-row keys above write them immediately, so they survive unplugging.

**They do not survive a reflash.** VIA derives its EEPROM magic from `QMK_BUILDDATE`, so every new
build invalidates the block and `eeconfig_init_user()` restores the defaults - lock off, LEDs on,
full brightness. That is the same mechanism that reloads your keymap automatically, so it's a
feature, not a bug, but re-set your preferences after flashing.

### Secure - pattern to unlock

**Default is off, deliberately.** QMK's Secure boots `SECURE_LOCKED` *and* re-locks after 60 s idle
by default. Wired straight to "swallow all keypresses" that is a keyboard which is dead on plug-in
and dead again every minute you stop typing. So:

- `SECURE_IDLE_TIMEOUT 0` - no auto-lock, ever. Locking is only ever explicit.
- The firmware calls `secure_unlock()` at boot **unless** the EEPROM `lock_on_boot` flag is set, and
  that flag ships off. Turn it on with the top-row key only after you've confirmed the pattern.

Flow when locked: every key is swallowed except the **top-left key**, matched on its matrix position
`(0,0)` so it works no matter which layer is active (layer keys are swallowed too). Press it →
`SECURE_PENDING` → type the pattern within 5 s → unlocked.

Pattern is `N → E → E → S` as matrix positions, in `config.h`:

```c
#define SECURE_UNLOCK_SEQUENCE {{3, 7}, {1, 3}, {1, 3}, {2, 2}}
```

**Change it.** And be clear-eyed about what it is: the pattern is in the firmware source and anyone
who can unplug the board can reflash it. It stops a passer-by; it is not security.

### Why the Claude lamps died with the backlight - and the fix

`quantum/rgb_matrix/rgb_matrix.c:421`:

```c
case RENDERING:
    rgb_task_render(effect);
    if (effect) {                                   // <-- 0 when RGB is off
        if (rgb_task_state == FLUSHING) rgb_matrix_indicators();
        rgb_matrix_indicators_advanced(&rgb_effect_params);
    }
```

Disabling the matrix sets `rgb_current_effect = 0`, and that `if (effect)` skips the indicator
callback entirely - so the three lamps went out with the key backlight.

Note this also rules out the obvious workaround: selecting `RGB_MATRIX_NONE` instead of disabling
doesn't help, because `RGB_MATRIX_NONE` **is** effect 0.

The fix is a custom effect, `CLAUDE_BLACKOUT`, that renders every key LED black but has a **non-zero
effect index**, so the indicator path keeps running. `RM_TOGG` is intercepted in `process_record_kb`
and switches to it instead of calling `rgb_matrix_disable()`; the previous mode is remembered in
EEPROM and restored on the next press. `keyboard_post_init_kb()` also force-enables the matrix,
since indicators render only while it runs.

Effects never touch LEDs 44-46 anyway: they carry `flags = 0`, and `RGB_MATRIX_TEST_LED_FLAGS()`
skips them. The flip side is that nothing else clears them either - so when there's no Claude state
to show, the indicator handler writes them black explicitly, or they'd hold a stale colour forever.

### The lamp status bus

The lamps are no longer Claude-specific. Four priority slots; the highest-priority live slot owns
all three lamps. Slot 0 is Claude Code, slots 1-3 are free for anything on the machine.

```bash
th40 status working                           # slot 0
th40 bus 1 blink red --ttl 60 --priority 95   # urgent, expires itself
th40 bus 2 breath amber --priority 15         # quiet background state
th40 clear 2
th40 selftest                                 # walk every pattern
```

Patterns `off solid breath pulse chase blink strobe`; colours by name or `#rrggbb`.
Give transient alerts a TTL - the firmware expires them locally, so a crashed script can't strand
the lamps lit and no follow-up call is needed.

### Why the colours looked wrong

Three separate causes, all fixed:

| Cause | Fix |
|---|---|
| Colours lighting all three channels read as **white** through the diffuser | saturated hues only - at least one channel at zero |
| A WS2812 green at 255 is perceptually several times brighter than blue, so a "traffic light" of raw 255s has a blinding green and a murky blue | per-channel gain: `R 205 / G 100 / B 255` |
| PWM is linear, sight is not - a linear breathe spends most of its time near full, which reads as cheap and flickery | gamma ≈ 2.0 on the animation curve |

**Blue was also just the wrong hue here.** On a diffused indicator it reads dim and cold next to
green and red, and no amount of gain fixes a hue problem. Working/loading are now **amber**, which
makes the set a literal traffic light: green = your turn, amber = busy, red = needs you.

`DONE` was a separate bug entirely: **no hook ever sent it.** `Stop` sent `idle`, so the finish
state could never fire. `Stop` now sends `done`, which strobes three times and expires to `idle`
after 2.2 s - finish, then "waiting for you", without the host sending a second message.

### The Claude Code plugin

Packaged at `/home/nees/key/plugins/th40`, listed by the local marketplace at
`/home/nees/key/.claude-plugin/marketplace.json`, and enabled in `~/.claude/settings.json` as
`th40@nees-local`.

| Path | What |
|---|---|
| `bin/th40` | the whole CLI - status, bus, scan-rate, selftest |
| `hooks/hooks.json` | all nine lifecycle hooks, `async` so they never delay a tool call |
| `commands/th40.md` | `/th40` slash command |
| `skills/th40-keyboard/SKILL.md` | loads the hard-won facts when working on the keyboard |

The hand-written hooks were **removed from `settings.json`** when the plugin took over - keeping
both would double-fire, and `Stop` would send `idle` and `done` at once. Plugin hooks load at
session start, so they take effect after a restart.

### Custom RGB effects

`RGB_MATRIX_CUSTOM_USER = yes`, defined in `keymaps/tapdance/rgb_matrix_user.inc`.

| Effect | Key | Behaviour |
|---|---|---|
| `CLAUDE_BLACKOUT` | `RM_TOGG` | keys dark, lamps alive - the "backlight off" state |
| `CLAUDE_AURA` | layer 3, top row | whole board takes the Claude state colour: breathing while thinking, a band sweeping left-to-right while a tool runs, a hard full-board strobe when permission is needed |
| `CLAUDE_RAIN` | layer 3, top row | Matrix rain in the Claude state colour, and **every keypress drops a fresh bright head on that column** |

The rain seeds itself from `claude_rain_kick[]`, written by `process_record_kb` with the exact
matrix column you hit - more precise than going through the hit tracker's pixel coordinates, and it
costs nothing since that hook already sees every keypress. Column heads advance in 1/16-row units on
a 28 ms tick, each at its own speed, and the simulation steps only when `params->iter == 0` so it
runs once per frame rather than once per render block.

State is shared with the effects through `keymaps/tapdance/claude_led.h` - the `.inc` is compiled
inside `rgb_matrix.c`, so `claude_state` has to be a real global, not a file-static.

### EEPROM writes are deferred

Holding a brightness key fires many changes a second, and this board's EEPROM is emulated in flash -
committing each one stalls the matrix scan and burns write cycles. Settings changes now mark a dirty
flag and `housekeeping_task_kb()` flushes once **750 ms** after the last change.

### Caps Word, combos, one-shots, swap hands

`caps_word_press_user()` keeps the word alive through `-` and `_` so `SCREAMING_SNAKE_CASE` and
`CONST-NAMES` survive; anything else ends it.

Combos (`COMBO_TERM 40`). The pairs matter more than the term - each is a digraph that barely occurs
in English, so fast typing can't fire them:

| Combo | Sends | Why this pair |
|---|---|---|
| `Q`+`W` | `Esc` | "qw" ~never occurs |
| `Z`+`X` | `Ctrl+Z` | "zx" ~never occurs |
| `C`+`V` | Caps Word | "cv" ~never occurs |
| `N`+`M` | `Delete` | "nm" rare |
| `M`+`,` | `-` | not a letter pair |
| `,`+`.` | `_` | not a letter pair |
| `J`+`K` | `Esc` | vim escape; "jk" ~never occurs |
| `.`+`'` | `:` | not a letter pair; `:` otherwise needs layer 2 + shift |
| `Q`+`P` | lock keyboard | opposite corners, needs both hands |

None touch the home-row-mod keys, so combos and mods can't interfere. `J`+`K` keeps that true by
being defined on the plain keycodes only - on `_HRM` those keys are `HM_J`/`HM_K`, so the combo
simply doesn't exist there.

One-shot mods use `ONESHOT_TIMEOUT 3000` and `ONESHOT_TAP_TOGGLE 2` - tap twice to lock a mod on.

### Tri layer, layer lock, key overrides, dynamic macros, key lock

- **Tri layer** lives in `layer_state_set_kb()`; holding both outer spaces opens `_MEDIA`. It is
  **not** `update_tri_layer_state()` any more, and that matters. That helper ends with

  ```c
  return (state & mask12) == mask12 ? (state | mask3) : (state & ~mask3);
  ```

  so it *clears* `_MEDIA` on every layer change where `_NAV`+`_NUM` are not both held. `LT(3,SPC)`
  on the `Fn` key set bit 3, the callback stripped it before the keypress resolved, and **the whole
  system layer was unreachable from `Fn`** - every documented "Fn + X" was dead, and the tri-layer
  chord was the only way in. Measured 2026-08-04. The replacement only ever *adds* `_MEDIA`, and
  removes it again only if a `tri_owns_media` ownership flag says the chord is what put it there.
  Any tri-layer written this way has the same trap - so does `TRI_LAYER_ENABLE`, which calls the
  same helper.
- **Layer lock** (`QK_LLCK`) sits on the quote-key position of `_NAV`, `_NUM`, `_MEDIA` and `_GIT`,
  replacing the `MO(0)` placeholders that did nothing. `LAYER_LOCK_IDLE_TIMEOUT 60000` releases a
  forgotten lock - a stuck layer is indistinguishable from a broken board.
- **Key overrides**: only `Shift+Backspace → Delete`. The trigger has to be the *literal* keymap
  keycode (`process_key_override.c` compares `override->trigger == keycode`), so the `LT(n,KC_SPC)`
  space bars cannot carry the classic shift+space→underscore - the `,`+`.` combo covers `_`.
- **Dynamic macros** (`DM_REC1/PLY1/REC2/PLY2` on `_GIT` home row, `DM_RSTP` below) - RAM only,
  cleared on reboot.
- **Key lock** (`QK_LOCK`, layer 3) pins the next basic keycode down until pressed again.

### Leader - the tmux prefix

Double-tap left Ctrl arms it; sequences are the if-chain in `leader_end_user()`. Design notes:

- Armed by **double-tapping left Ctrl** (`TD_CTL`, built like the shift dance: Ctrl registers on
  keydown, so holds and chords cost nothing; only a clean tap-tap arms the prefix). Double-tap
  *letters* were requested twice (`a`, then `f`) and rejected both times: a letter dance either
  delays every press of that letter or must backspace what it typed, and "ff" appears in
  off/coffee/different - it would arm mid-word constantly. A modifier gives the double-tap feel
  for free. An `F`+`J` combo was the interim trigger before this.
- `LEADER_NO_TIMEOUT` + `LEADER_PER_KEY_TIMING 300`: armed waits forever (tmux behaviour), then
  each sequence key buys 300ms. Core leader has no early-match - the action always fires one
  timeout after the last key. Don't chase that lag; it's structural.
- `leader_start_user()` parks solid cyan on **bus slot 3** at priority 60 (above idle/working,
  below permission/done/error) and `leader_end_user()` clears it - so an armed prefix is always
  visible, and an accidental `F`+`J` explains itself instead of silently eating keys.

### Auto Shift and Autocorrect - removed by choice

Both were built, worked, and were then removed at the owner's request (`= no` in `rules.mk`).
The code paths remain behind `#ifdef` guards, so re-enabling is a one-line `rules.mk` edit - the
~83 KB ceiling is no obstacle with LTO on. Two findings from that work stay relevant:

- **Never write EEPROM in `keyboard_post_init_kb`.** `autocorrect_enable()/disable()` call
  `eeconfig_update_keymap()`. Autocorrect state persists natively in `keymap_config` anyway -
  QMK's own eeconfig default is even "enabled" - so keymap-side persistence for it is redundant
  *and* dangerous. Auto Shift has no native persistence; its opt-in flag lives in the user
  datablock, synced by `feature_state_sync()` via the deferred-write path, and applied at boot
  with the RAM-only `autoshift_disable()`.
- **`EECONFIG_USER_DATA_SIZE` is pinned at 4 by the keyboard's `config.h`** and redefining it
  differently is a `-Werror` (an *identical* redefinition is legal, which is why 4 worked
  before). New flags fit by packing the booleans in `user_config_t` as bitfields.

`hand_swap_config` mirrors **each row across its own centre**, not a blanket `11 - col`: the rows
have different key spans on this board (row 1 is cols 1-11, row 2 is cols 0-10, row 4 is sparse), so
a uniform mirror would map several keys onto holes.

### Home row mods (GACS)

`A S D F` = GUI / Alt / Ctrl / Shift, mirrored on `J K L`. Three settings make this usable
rather than infuriating:

| Setting | Why |
|---|---|
| `CHORDAL_HOLD` | Same-hand chords settle as **taps**, so rolling `df` types "df" instead of firing Ctrl. Handedness comes from `chordal_hold_layout` in `keymap.c`; the three thumbs are `'*'` so they chord with either hand. |
| `FLOW_TAP_TERM 150` | While you're actually typing, holds are disabled outright - **no home-row-mod latency mid-word**. Mods only engage after a pause. |
| `PERMISSIVE_HOLD` | Opposite-hand mods engage without waiting out the tapping term. Safe here because Chordal Hold already guards same-hand rolls. |
| `QUICK_TAP_TERM 0` | Holding `A` after tapping it gives GUI, not "aaaa". Key repeat moved to `QK_REP`. |

Tapping term is **130 ms globally** (what the `LT()` keys were tuned to) with **180 ms** for the
seven mod keys and **200 ms** for the shift dance, via `get_tapping_term()`.

`is_flow_tap_key()` is overridden. The stock version inspects the *tap* keycode, so it treats
`LT(2,KC_SPC)` as a typing key and would swallow the space-layers whenever you reached for one
straight after a letter. The override exempts all layer-taps.

Want mods to engage even harder? Add `#define HOLD_ON_OTHER_KEY_PRESS` - safe alongside
Chordal Hold, at the cost of more misfires on fast opposite-hand rolls.

### Typing latency: what was already optimal

Worth recording so it isn't "optimised" again pointlessly:

| | State | Verdict |
|---|---|---|
| Debounce algorithm | `asym_eager_defer_pk` | **Already zero added press latency** - eager press fires on the first edge; only the release is deferred. This is also the correct anti-chatter design, so raising `DEBOUNCE` would add latency and fix nothing. |
| USB poll rate | `bInterval 1` = 1000 Hz | Already maxed (confirmed in the descriptors) |
| MCU idle | `CORTEX_ENABLE_WFI_IDLE FALSE` | Already disabled, no wake latency |

The only remaining variable is matrix scan rate, which the RGB flush and the wireless SPI can
starve. Read it with:

```bash
th40-claude-status scan-rate      # -> "N matrix scans/sec"
```

**Measured 2026-08-03: 3083 scans/sec** (0.32 ms per scan) with RGB running; **5314 scans/sec**
later the same day once `LTO_ENABLE = yes` landed. Full input latency
is therefore ~0.16 ms scan + 0 ms debounce + ~0.5 ms USB = **~0.7 ms average, ~1.3 ms worst case**,
which is the floor for USB HID. There is no latency here left to reclaim - don't go looking.

If it ever drops below ~1000/s, the knobs are `RGB_MATRIX_LED_FLUSH_LIMIT` and
`RGB_MATRIX_LED_PROCESS_LIMIT` in the keyboard's `config.h`. Compare against RGB off (`RM_TOGG`)
to isolate the lighting cost.

### Tap dance: double-tap shift → Caps Lock

Lives on left shift (matrix `3,0`) on layers 0-2. Layer 3 leaves it transparent.

| Action | Result |
|---|---|
| Hold shift + type | Shift, **zero added latency** |
| Double-tap shift | Caps Lock toggles |
| Double-tap-and-hold | Normal shift |

The obvious implementation, `ACTION_TAP_DANCE_DOUBLE(KC_LSFT, KC_CAPS)`, is wrong for a shift key:
it waits out the tapping term before shift fires, which wrecks fast typing. Instead:

- `on_each_tap` → `register_code(KC_LSFT)` on the **keydown**, so shift is instant
- `on_each_release` → `unregister_code(KC_LSFT)` on the **keyup**, so it can never stick into the
  next keystroke
- `finished` → `tap_code(KC_CAPS)` only when `count == 2 && !pressed`

Typing `Shift+H` can't false-trigger Caps: the letter's keypress interrupts the dance, which
finishes it at `count == 1` and resets on release, so the next shift press starts a fresh dance.

Tapping term is **130 ms globally** (the board default - your `LT(1,KC_TAB)`, `OSL(2)` and the three
`LT(n,KC_SPC)` spaces are tuned to it) with a **200 ms** per-key override for the tap dance only, so
the double tap is comfortable without loosening the layer taps. That's `TAPPING_TERM_PER_KEY` in
`keymaps/tapdance/config.h` plus `get_tapping_term()` in `keymap.c`.

`keymaps/tapdance/rules.mk` is the whole feature switch - VIA/dynamic keymap, tap dance, repeat,
Caps Word, combos, Secure, swap hands, custom RGB, key overrides, layer lock, key lock, dynamic
macros, Auto Shift and Autocorrect. One line per feature; delete a line to drop the feature.

### Known quirk in the current layout

The **bottom-right three keys** (the Alt / Menu / Ctrl positions right of the third space -
matrix `4,8` `4,9` `4,10`) arrived as `KC_NO` on layer 0 from the VIA export, i.e. dead. They now
carry `KC_F21`/`F22`/`F23` (the Claude keys) on layer 0, `MO()` for layers 4-6 on layer 1, and
`TG()` for the same three on layer 3. Nothing on this board is dead any more.

---

## 4. Build

```bash
cd /home/nees/key/qmk
make -j$(nproc) epomaker/th40:tapdance
```

Output: `/home/nees/key/qmk/.build/epomaker_th40_tapdance.bin` (also copied to the repo root).

Watch the size line - but the limit that matters is **not** the 128 KB of flash. Measured
2026-08-03, the hard way:

| Image (real code bytes) | Boots? |
|---|---|
| 70,144 (factory) · 81,098 · ~82.8 K | yes |
| 84,912 · 85,572 · 87,706 · 88,352 | **no - board enumerates as nothing at all** |
| 81,098 padded to 88,438 with `0xFF` | yes - **which proves nothing**: the bootloader skips blank data, so padding cannot probe the ceiling |

So treat **~83 KB of real code as the boot ceiling**. `LTO_ENABLE = yes` in the keymap's
`rules.mk` keeps the full feature set at ~75 KB (and, as a bonus, raised the scan rate from
~3,100 to ~5,300 scans/sec). A too-big image is not a brick: the physical bootloader sequence
still works, flash anything smaller.

Four theories were tested and disproven before the size ceiling was found, in this order: a
corrupted write (reflash - same failure), RAM overflow (data+bss always reads ~16,380/16,384 on
this port because the heap absorbs the remainder - it's padding, not pressure), app image
overlapping the EEPROM emulation pages at 0x1C000 (an 88 K image ends ~20 KB short of them), and
a per-file bootloader write cap (killed by the padded-image test, which then turned out to be
confounded anyway). Recorded so nobody walks that path again.

---

## 5. Flash

### 5a. Enter the bootloader

**Physical method (always works):**

1. Back switch → **wired/USB** position
2. **Unplug** the cable
3. **Hold the top-left key** (matrix `0,0` - labelled Esc, mapped to `QK_GESC`)
4. **Plug the cable back in** while holding, release after ~2 s

There's also a reset button on the back of the PCB, but that means opening the case.

**There is currently no working software method - the physical sequence above is required.**
Three things were tried, all documented here so they aren't re-attempted blindly:

| Attempt | Result |
|---|---|
| VIA `id_bootloader_jump` (`0x0B`) on stock firmware | `ff` - Epomaker stripped the handler |
| VIA `id_bootloader_jump` on our QMK build | `ff` - this fork's `via.c` never implements it (`grep bootloader quantum/via.c` returns nothing) |
| Our own `0xC1 0xB0` → `bootloader_jump()` | Board **reboots but re-enters the application**, not the bootloader |

The third is implemented and still in the keymap (`CLAUDE_SUB_BOOTLOADER`, chosen far outside the
`0..6` status range so a stray LED update can never reboot the board). It is *not* wired into
`flash-th40.sh` by default - run with `TH40_TRY_SOFT_JUMP=1` to exercise it.

**Next thing to try:** the vendor's `bootloader_jump()` in `lib/rdmctmzt_common/user_system.c` spins
on `Spi_Send_Recv_Flg` / `ES_SPI_ACK_IO` / `Reset_Save_Flash` before remapping flash to boot. We call
it from `via_command_kb()`, which runs in the USB callback context, where those flags likely never
settle - so it times out after 36000 iterations and resets *without* the remap. Deferring the call to
the main loop (set a flag in `via_command_kb()`, act on it in a `housekeeping_task_kb()` override
that still calls `housekeeping_task_user()`) is the standard fix for exactly this class of problem.

### 5b. Confirm it's in the bootloader

```bash
lsusb | grep 03eb:2045          # -> "Atmel Corp. LUFA Mass Storage Demo Application"
lsblk -o NAME,SIZE,FSTYPE,MODEL # -> sda, ~106K, vfat, "RDMCTMZT DFU"
```

It is **mass storage, not DFU** - `dfu-util` will never see it, despite the "RDMCTMZT DFU" product
string. QMK Toolbox calls this bootloader class "LUFA MS".

### 5c. Write the firmware

```bash
udisksctl mount -b /dev/sda                      # mounts at /run/media/nees/<UUID>
cp /home/nees/key/qmk/.build/epomaker_th40_tapdance.bin /run/media/nees/<UUID>/FIRMWARE.BIN
sync
udisksctl unmount -b /dev/sda
```

The board flashes on write and reboots itself - errors from `sync`/`unmount` at that point are
normal, not a failure. Confirm the block device is the keyboard before writing; `/dev/sda` is only
correct because this machine's real disk is NVMe.

Or use the helper: `./flash-th40.sh`

### 5d. Verify the right firmware is running

Don't trust the reboot - check the USB identity, which comes from the firmware itself:

```bash
lsusb -d 36b0:304e -v 2>/dev/null | grep -E "bcdDevice|iManufacturer"
```

| | stock | ours |
|---|---|---|
| `iManufacturer` | `RDMCTMZT` | `EPOMAKER` |
| `bcdDevice` | `0.05` | `0.04` |

### 5e. EEPROM

**No EEPROM clear is needed after a rebuild.** VIA's EEPROM validity magic is derived from
`QMK_BUILDDATE` (`quantum/via.c:70`), so every new build invalidates the stored keymap and
`dynamic_keymap_reset()` reloads it from the compiled one automatically.

The corollary: **anything remapped in the VIA app is wiped by the next flash.** Compile changes into
`keymap.c` rather than clicking them into VIA. And don't rewrite the shift key in VIA at all - that
replaces the tap dance with a plain keycode.

---

## 6. Recovery

Factory firmware is saved at:

```
/home/nees/key/TH40_factory_firmware.zip
```

(`EPOMAKER TH40-三模-机械键盘-CS636E-V0104-20250226.bin`, from the fork's
[0.31.2 release](https://github.com/carlosedp/qmk_firmware/releases/tag/0.31.2).)

A bad flash is not a brick: bootloader entry is a hardware key combo that doesn't depend on the
firmware booting. Hold the top-left key while plugging in, and write the factory `.bin` the same way
as §5c.

---

## 7. Raw HID: the 32-byte gotcha

**QMK reads exactly `RAW_EPSIZE` (32) bytes and silently ignores anything shorter.** With hidapi on
Linux that means writing **33** bytes: one report-ID byte (`0x00`, since QMK uses no report IDs)
plus a full 32-byte payload.

```python
h.write(bytes([0x00, 0x01] + [0x00] * 31))   # 33 bytes total - correct
h.write(bytes([0x00, 0x01] + [0x00] * 30))   # 32 total - firmware never answers
```

This cost real time once: the short write made a perfectly healthy firmware look dead on the VIA
channel. The stock Epomaker firmware tolerated the short report, so the bug only appeared after
switching to QMK. If raw HID ever "stops responding", check the payload length first.

Handy probe:

```bash
python3 -c "
import hid
p=[d for d in hid.enumerate(0x36b0,0x304e) if d['usage_page']==0xFF60][0]
h=hid.Device(path=p['path']); h.write(bytes([0x00,0x01]+[0x00]*31))
print(h.read(32,1000)[:3].hex(' '))"     # -> 01 00 0d  (VIA protocol 13)
```

---

## 8. Claude Code status LEDs

The three lamps above the key grid (LEDs **44 / 45 / 46**) mirror what Claude Code is doing. They
sit outside the key matrix with `flags = 0`, so RGB animations skip them - which is exactly why
they're free to repurpose.

| State | LEDs | Fired by |
|---|---|---|
| `idle` | slow dim coral breath | `SessionStart`, `Stop` |
| `working` | coral pulse | `UserPromptSubmit`, `PostToolUse` |
| `loading` | `. . .` coral chase | `PreToolUse` |
| `permission` | fast amber blink | `Notification` |
| `done` / `error` | green / red, self-expiring after 2.5 s | (not yet wired to a hook) |
| `off` | LEDs return to the stock indicators | `SessionEnd` |

Caps Lock still wins LED 44 whenever it's on. `DONE`/`ERROR` expire to `idle` on their own, and a
30-minute watchdog drops everything to `off` if the host stops talking, so a crashed session can't
strand the lights.

**Pieces:**

| | |
|---|---|
| Firmware | `via_command_kb()` + `rgb_matrix_indicators_advanced_user()` in `keymaps/tapdance/keymap.c` |
| Wire protocol | raw HID, `[0xC1, state]` - command `0xC1` is outside VIA's `0x01..0x15` range |
| Host sender | `~/.local/bin/th40-claude-status <state>` |
| Hooks | `~/.claude/settings.json`, all `async: true` so an LED update never delays a tool call |

`th40.c` needed a one-line patch for this: its `rgb_matrix_indicators_advanced_user` was a strong
symbol, so a keymap couldn't override it. It's now `__attribute__((weak))`, and our version calls
`kb_rgb_matrix_indicators_common()` first to keep battery/mode/caps feedback. **A `git pull` in the
fork will revert that** - reapply it if the LEDs stop responding after an update.

Manual test, independent of Claude Code:

```bash
th40-claude-status loading --verify   # -> "sent loading -> keyboard is now in state 3"
th40-claude-status off
```

If `--verify` prints `not handled (byte0=0xFF)`, the running firmware predates this feature.

### The three Claude keys

The formerly-dead bottom-right keys (matrix `4,8` / `4,9` / `4,10`) now send **F21 / F22 / F23**,
bound in `~/.config/hypr/bindings.conf` to `~/.local/bin/claude-key`:

| Key | Action |
|---|---|
| F21 | focus the terminal running Claude Code, or launch one |
| F22 | send Return to it - accept a permission prompt |
| F23 | send Escape to it - interrupt |

`claude-key` locates the window with `hyprctl clients -j`, preferring an `Alacritty` window whose
title matches `claude` and falling back to any terminal, so approve/interrupt work from the browser
too. If you switch terminals, change `TERM_CLASS` at the top of that script.

---

## 9. Ideas for next time

`TAP_DANCE_ENABLE` is already on, and there's ~56 KB of flash headroom. Worth considering:

- `CAPS_WORD_ENABLE` - auto-shifts one word, often what people actually want from double-tap shift
- `COMBO_ENABLE` - chords, which suit a 40% well
- Wire `done` / `error` to a hook - `PostToolUseFailure` is the obvious home for `error`
- More dances (the code is written generically; adding one is an enum entry plus a `tap_dance_actions[]` row)
