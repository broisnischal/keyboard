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
only via the physical combo). `LTO_ENABLE = yes` is now mandatory; current build ~75 KB. Do not
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

## Verifying, rather than assuming

The keyboard is the source of truth. Read it back:

```bash
lsusb -d 36b0:304e -v 2>/dev/null | grep -E "bcdDevice|iManufacturer"  # EPOMAKER / 0.04
th40 config          # persistent settings + lock state
th40 scan-rate       # ~3100/sec is healthy
th40 selftest        # walk every lamp pattern
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
