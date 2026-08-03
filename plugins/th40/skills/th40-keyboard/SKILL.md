---
name: th40-keyboard
description: Use when working on the Epomaker TH40 keyboard - building or flashing its QMK firmware, editing the keymap or layers, changing the indicator lamps, or pushing status to the keyboard's LED bus from a script or long-running job.
---

# Epomaker TH40

The full runbook is `/home/nees/key/keyboard.md`. **Read it before changing firmware** rather than
re-deriving anything — it records several findings that cost real time to discover.

## Facts worth not rediscovering

- Upstream QMK has **no ES32/FS026 port**. The only working source is the fork
  `github.com/carlosedp/qmk_firmware`, cloned at `/home/nees/key/qmk`.
- The bootloader is **LUFA mass-storage** (`03eb:2045`), not DFU. You copy `FIRMWARE.BIN` onto a
  mounted volume. `dfu-util` will never see it despite the "RDMCTMZT DFU" product string.
- **There is no working software bootloader entry.** Flashing always needs the physical
  hold-top-left-key-while-plugging-in sequence. Three approaches were tried and documented.
- QMK raw HID reads **exactly 32 bytes**. With hidapi that means writing **33** (report ID + 32).
  A short write makes healthy firmware look completely dead.
- `rgb_matrix.c` skips the indicator callback when the effect index is `0`, which is why the lamps
  used to die with the backlight. The fix is the non-zero `CLAUDE_BLACKOUT` effect.

## Build and flash

```bash
cd /home/nees/key/qmk && make -j$(nproc) epomaker/th40:tapdance
/home/nees/key/flash-th40.sh          # waits for the bootloader, writes, verifies
```

Watch the size line: **128 KB is the hard ceiling.**

Verify the right firmware is running — this comes from the firmware itself, so it is the only
trustworthy check:

```bash
lsusb -d 36b0:304e -v 2>/dev/null | grep -E "bcdDevice|iManufacturer"   # EPOMAKER / 0.04
```

## The lamp bus

Three lamps, four priority slots. Slot 0 is Claude Code; 1–3 are free.

```bash
th40 status working                              # slot 0
th40 bus 1 blink red --ttl 60 --priority 95      # urgent, expires itself
th40 bus 2 breath amber --priority 15            # low-key background state
th40 clear 2
th40 scan-rate                                   # matrix scans/sec
th40 selftest                                    # walk every pattern
```

Patterns: `off solid breath pulse chase blink strobe`.
Colours: `red green amber orange yellow cyan blue violet pink` or `#rrggbb`.

**Use saturated colours only.** Anything lighting all three channels reads as white through the
diffuser — that is a property of the hardware, not a preference.

Higher priority wins. Give a transient alert a TTL so it clears itself rather than needing a
follow-up call; the firmware expires it locally, which also means a crashed script can't strand the
lamps lit.

## When editing the keymap

The keymap lives in `qmk/keyboards/epomaker/th40/keymaps/tapdance/`. **Do not modify the fork** —
`th40.c` owns the `*_user` callbacks, so the keymap overrides the `*_kb` variants and calls through.
That keeps `git status` clean and survives a `git pull`.

Every layer must have exactly **44 entries**. Quick check:

```bash
python3 - <<'EOF'
import re
s=open("qmk/keyboards/epomaker/th40/keymaps/tapdance/keymap.c").read()
print([len([x for x in re.split(r',(?![^(]*\))', m.group(1)) if x.strip()])
       for m in re.finditer(r'LAYOUT_tkl_ansi\((.*?)\n\s*\)', s, re.S)])
EOF
```
