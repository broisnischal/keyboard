# CLAUDE.md

Custom QMK firmware for an **Epomaker TH40**, plus the host tooling that drives its three
indicator lamps from Claude Code.

## Orientation

| File | What it is |
|---|---|
| `keymap/` | **The actual work.** Symlinked into the QMK tree; this is what git tracks. |
| `qmk/` | 1.4 GB clone of the fork. **Gitignored** - recreate with `./setup.sh`. |
| `keyboard.md` | Build/flash runbook and every hard-won finding. **Read before touching firmware.** |
| `docs.md` | End-user guide: what each feature is and how to use it. |
| `plugins/th40/` | Claude Code plugin - the `th40` CLI, `/th40` command, a skill. |
| `flash-th40.sh` | Waits for the bootloader, writes, verifies. |
| `chatter-watch.py` | Prints what the board actually emitted and flags physically impossible repeats. Settles "is this switch chatter or my firmware?" |
| `setup.sh` | Rebuilds the environment from a fresh clone. |
| `TH40_factory_firmware.zip` | Stock firmware, for rolling back. |
| `th40-via-keymap-backup.json` | The original VIA export the keymap was derived from. |
| `th40-via-definition.json` | VIA definition with the custom effects added to the dropdown. |

## Working here

```bash
./setup.sh                       # first time, or after cloning
cd qmk && make -j$(nproc) epomaker/th40:tapdance
./flash-th40.sh                  # then do the physical bootloader sequence
```

**Never edit inside `qmk/`.** `th40.c` owns the `*_user` callbacks, so the keymap overrides the
`*_kb` variants and calls through. The fork is unmodified and must stay that way - `git -C qmk
status` should show only the untracked symlink.

**The real ceiling is ~83 KB of code, not 128 KB of flash.** Every image over ~83 KB failed to
boot (measured 2026-08-03: 81-83 KB boots, 84.9 KB+ dead - board enumerates as nothing, bootloader
only via the physical combo). `LTO_ENABLE = yes` is now mandatory; current build ~74 KB. Do not
test the ceiling by padding with `0xFF` - the bootloader skips blank data and the test lies.

Every layer must have exactly 44 entries:

```bash
python3 - <<'EOF'
import re
s=open("keymap/keymap.c").read()
print([len([x for x in re.split(r',(?![^(]*\))', m.group(1)) if x.strip()])
       for m in re.finditer(r'LAYOUT_tkl_ansi\((.*?)\n\s*\)', s, re.S)])
EOF
```

## Things that cost real time - do not rediscover them

- **Upstream QMK has no ES32/FS026 port.** Only `github.com/carlosedp/qmk_firmware` works.
- **The bootloader is LUFA mass-storage** (`03eb:2045`), not DFU. You copy `FIRMWARE.BIN` onto a
  mounted volume. `dfu-util` will never see it despite the "RDMCTMZT DFU" product string.
- **There is no working software bootloader entry.** Three approaches were tried and all failed;
  `keyboard.md §5a` records which, so they are not re-attempted. Flashing always needs the physical
  hold-top-left-key-while-plugging-in sequence.
- **QMK raw HID reads exactly 32 bytes.** With hidapi that means writing **33** (report ID + 32). A
  short write makes healthy firmware look completely dead.
- **VIA replies put the payload at different offsets.** `id_dynamic_keymap_get_keycode` returns the
  keycode at bytes 4-5; `id_custom_get_value` returns its value at byte **3**. Misreading this once
  produced a completely wrong diagnosis.
- **`rgb_matrix.c` skips the indicator callback when the effect index is 0.** So disabling the
  matrix - or selecting `RGB_MATRIX_NONE` - kills the Claude lamps. `CLAUDE_BLACKOUT` exists to be a
  non-zero effect that renders black.
- **VIA cannot discover custom effect names.** There is no protocol message for it. Custom effects
  only appear in VIA if the definition JSON lists their indices explicitly.
- **Every reflash resets the EEPROM.** VIA derives its magic from `QMK_BUILDDATE`, so the keymap
  reloads automatically (good) and user settings return to defaults (expected).
- **Only one flasher at a time.** Two waiting instances both wake on the same bootloader and both
  write `FIRMWARE.BIN`, corrupting the image. `flash-th40.sh` now holds a `flock`.
- **`EECONFIG_USER_DATA_SIZE` is pinned at 4 by the keyboard's `config.h`.** Redefining it
  differently in the keymap is a `-Werror`; an identical redefinition is legal. New persistent
  flags go into `user_config_t` as bitfields, not extra bytes.
- **No EEPROM writes in init paths.** `autocorrect_enable()/disable()` call
  `eeconfig_update_keymap()`; anything like that belongs in the main loop, never in
  `keyboard_post_init_kb`. Autocorrect/one-shot/etc. state persists natively in `keymap_config` -
  don't duplicate it in the user datablock.
- **A dead board after a flash is almost always the ~83 KB boot ceiling.** Diagnose in this order:
  `lsusb -d 36b0:304e` (app), `lsusb -d 03eb:2045` (bootloader), size of the last image. Recovery
  is always the physical Esc-and-plug sequence plus a smaller image.
- **`update_tri_layer_state()` silently steals the third layer.** It ends with `(state & ~mask3)`,
  so it *clears* the adjust layer on every layer change where the two trigger layers are not both
  held. That killed `LT(3,KC_SPC)` on the Fn key outright - the system layer was reachable only by
  the tri-layer chord, and every "Fn + X" in `docs.md` was dead for a month. `layer_state_set_kb()`
  now only ever ORs the bit in, with a `tri_owns_media` flag deciding when it may come off.
  `TRI_LAYER_ENABLE` calls the same helper and has the same bug.
- **A default layer above an overlay makes that overlay unreachable.** `layer_switch_get_layer()`
  scans `layer_state | default_layer_state` from the highest index down, so `_HRM` parked at 7 - a
  persisted default layer with no transparent keys - answered every lookup first and killed layers
  1-6 outright. Worse, the way back (`Fn`+`T` = `UC_HRM`) resolved to plain `T`, so the keyboard
  could not undo it; recovery was writing `UC_HRM` into the dynamic keymap over raw HID. `_HRM` is
  index 1 now and every layer reference in `keymap.c` is symbolic. Never write `LT(2,...)`.
- **Echo-check every VIA read.** Rapid back-to-back raw HID calls on one handle can return the
  *previous* request's reply, which reads as a scrambled keymap and sends you diagnosing a
  non-existent bug. `id_dynamic_keymap_get_keycode` echoes layer/row/col at bytes 1-3 - assert them.
- **`th40 lock-on-boot on|off` hangs.** `_send()` blocks on a reply the firmware never sends for
  `SUB_CFG_SET`. `config`, `scan-rate` and `unlock` are fine. Not yet fixed.
- **Key overrides match the literal keymap keycode.** An `LT(n,KC_SPC)` space bar can never
  trigger a `KC_SPC` override - don't re-attempt shift+space→underscore.
- **The drawing is not evidence that a key works.** `KC_NUBS` sat on the Numbers layer for a month
  as the backslash key; it is the *ISO* extra key, unmapped in the `us` xkb layout, so it typed
  nothing - while keymap-drawer rendered it "\ |" and made the drawing look right. Worse, the two
  keycodes are drawn *inversely*: keymap-drawer gives `KC_NUBS` a `|` shifted legend and `KC_BSLS`
  none, so fixing the firmware silently deleted `|` from the picture. `postprocess.py` re-adds it.
  Confirm a symbol against the host layout, not the SVG.
- **Three features were removed for latency, not taste. Don't helpfully add them back.** All three
  were reported as "typing feels like there's a little lag" and all three were confirmed in the QMK
  source, 2026-08-05:
  - **Combos cost latency but are KEPT - do not remove them again.** `process_combo()` buffers the
    keydown of every key belonging to any combo until the combo is ruled out (next keydown, release,
    or `COMBO_TERM`), and the nine here cover `Q W Z X C V N M , . ' J K P`. Picking rare digraphs
    prevents false triggers, not the delay. They were removed once as a latency fix that was never
    asked for, and had to be restored. `COMBO_TERM` is the only legitimate knob.
  - **Leader** (`LEADER_ENABLE = no`). Arming on a double-tap forces the trigger to be a tap dance;
    it was on left Ctrl, so every Ctrl chord sat inside a 200 ms state machine. Registering the mod
    in `on_each_tap` makes it *functionally* instant and it still felt late. A leader needs a
    dedicated key or nothing.
  - **`LT(_NAV,KC_TAB)`** broke `Alt`+`Tab`: a tap-hold emits its tap on **release**, so nothing
    fired until the finger came up, and holding Tab past 130 ms gave the layer and no Tab at all.
    Tab was made plain, then the hold was restored on request - see the `pre_process_record_kb` note
    below, which is what makes both work at once.
- **`PERMISSIVE_HOLD` must not apply to the `LT(n,KC_SPC)` thumbs.** Its rule - held tap-hold key,
  another key pressed *and released*, resolve as hold - is exactly the shape of rolling through the
  space bar, so a fast roll produced a digit instead of "space letter". `CHORDAL_HOLD` cannot catch
  it: the thumbs are `'*'` in `chordal_hold_layout` by design. `get_permissive_hold()` returns
  `IS_QK_MOD_TAP(keycode)` and nothing else. `PERMISSIVE_HOLD_PER_KEY` wins over the bare
  `PERMISSIVE_HOLD` in `action_tapping.c`, so only the per-key define is present.
- **Never give a tap-hold key a term shorter than an ordinary press of its tap.** The three
  `LT(n,KC_SPC)` thumbs sat at the global `TAPPING_TERM` of 130 ms for two weeks. A thumb rests on
  the space bar - 130-250 ms is a normal space - so the term expired *on its own*, with nothing else
  pressed, and the space resolved as a **hold**: the layer came on silently, the space vanished (a
  hold has no tap, so words ran together), and the next letter was read off that layer. `O` became
  `9` on `_NUM`, nothing at all on `_MEDIA`/`_NAV`, and `_NAV`+`B` / `_MEDIA`+`F` are `QK_REP`, the
  only thing in this firmware that can duplicate a character. One mechanism, and it presented as
  three separate faults: missing letters, doubled letters, and lag. Measured 2026-08-17. The fix is
  `THUMB_TAPPING_TERM 230` + `RETRO_TAPPING_PER_KEY` + `FLOW_TAP_TERM_THUMB 110`; none of the three
  costs latency, because **a tap-hold emits its tap on RELEASE** - a 90 ms space lands at 90 ms
  whatever the term is. `keyboard.md §3` has the per-thumb damage table. Corollary: `OSL()` on a key
  you can brush needs a short `ONESHOT_TIMEOUT` for the same reason (3000 → 1200).
- **"Some keys double type" is a firmware claim until measured.** `./chatter-watch.py` reads the
  board's own evdev node and flags any key re-pressed under 40 ms after its own release, which no
  finger can do. No hits means switch chatter is ruled out and `DEBOUNCE` is the wrong knob - go
  looking for `QK_REP`, `QK_LOCK` or `DM_PLY*` reachable by an accidental layer instead.
- **A tap-hold key cannot be fixed from `process_record_kb` - it runs too late.** For a tap-hold
  key, `action_tapping.c` buffers the keydown and calls `process_record()` only once it has *decided*,
  so `record->tap.count` checks there happen after any latency is already spent. The hook that runs
  early is **`pre_process_record_kb()`**, called from `action_exec()` at `action.c:133` before
  `action_tapping_process()`; returning false from it skips tap-hold entirely for that event. That is
  how `LT(_NAV,KC_TAB)` keeps its Nav hold while `Alt`+`Tab` still fires on the keydown - a held
  modifier short-circuits to `register_code(KC_TAB)`. Guard any such hook with `secure_is_locked()`:
  `pre_process` runs *before* the lock swallow in `process_record_kb`, so without it a locked board
  still emits keys.
- **`DYNAMIC_KEYMAP_LAYER_COUNT` is 8** (`lib/rdmctmzt_common/fs026_eeprom.h`), and
  `keymap_introspection.c` static-asserts `keymaps[]` against it. Eight layers is a hard ceiling, not
  a target. `_SPR1`/`_SPR2` are declared and fully transparent so the count is accounted for and no
  index below them ever has to move.

## Verifying, rather than assuming

The keyboard is the source of truth. Read it back:

```bash
lsusb -d 36b0:304e -v 2>/dev/null | grep -E "bcdDevice|iManufacturer"  # EPOMAKER / 0.04
th40 config          # persistent settings + lock state
th40 scan-rate       # 5000/sec with LTO; anything over ~1000 is healthy
th40 selftest        # walk every lamp pattern
./chatter-watch.py   # type a sentence + Enter; prints what the board really sent
```

Reading a keycode straight out of the keyboard's EEPROM, which is how every claim about the keymap
in this repo was checked:

```python
import hid
p=[d for d in hid.enumerate(0x36B0,0x304E) if d['usage_page']==0xFF60][0]
h=hid.Device(path=p['path'])
h.write(bytes([0x00,0x04,layer,row,col]+[0x00]*28))   # 33 bytes total
r=h.read(32,1500); print(hex(r[4]<<8 | r[5]))
```

## Host side

Hooks live in `~/.claude/settings.json`, **not** in the plugin. Plugin hooks only load at session
start; settings.json hot-reloads. The plugin's `hooks/` directory was deliberately deleted so there
is exactly one source and no double-firing.

The plugin is registered via `.claude-plugin/marketplace.json` here and enabled as `th40@nees-local`.

## Style

The three docs have distinct jobs - keep them that way. `keyboard.md` is for whoever maintains the
firmware, `docs.md` is for whoever types on it, this file is for whoever picks up the work.

State what was measured and what was assumed. Several findings above are recorded specifically
because a confident guess was wrong the first time.
