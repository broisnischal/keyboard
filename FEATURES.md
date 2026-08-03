# Epomaker TH40 — what this keyboard can do

44 keys, custom QMK firmware, and more features than most full-size boards. This is the showcase;
the how-to lives in [`docs.md`](docs.md), the build guide in [`keyboard.md`](keyboard.md).

```
 ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
 │Esc │ Q  │ W  │ E  │ R  │ T  │ Y  │ U  │ I  │ O  │ P  │ ⌫  │
 ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┴────┤
 │Tab │ A  │ S  │ D  │ F  │ G  │ H  │ J  │ K  │ L  │  Enter  │
 ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┬────┤
 │ ⇧  │ /  │ Z  │ X  │ C  │ V  │ B  │ N  │ M  │ ,  │ .  │ '  │
 ├────┼────┼────┼────┴────┴──┬─┴────┼────┴──┬─┴────┼────┼────┤
 │Ctrl│Gui │Alt │   Space L  │  Fn  │Space R│ ◆  │ ✓  │ ✕  │
 └────┴────┴────┴────────────┴──────┴───────┴────┴────┴────┘
```

## The party trick: it talks to Claude Code

Three lamps above the keys mirror what Claude Code is doing, live over raw HID:

| Lamps | Meaning |
|---|---|
| green breath | waiting for a prompt |
| amber pulse / chase | thinking / running a tool |
| **red blink** | **needs approval** — red is reserved, it always means look |
| green triple-flash | done |

The three bottom-right keys (`◆ ✓ ✕`) focus the Claude terminal, approve a permission prompt, or
interrupt it — from anywhere, even mid-browser. The lamps are also a general four-slot priority
bus any script can drive (`th40 bus 1 blink red --ttl 60`): low disk, failed build, whatever.
Whole-board effects can mirror the lamps (**Aura**) or rain in the state colour, seeded by your
own keystrokes (**Rain**).

## A leader key, like tmux's Ctrl-b

Press `F`+`J` together — the lamps turn cyan, the keyboard is armed and waits. Then one short
sequence runs an action:

`S` sleep the PC · `P`/`N`/`B`/`M` music transport and mute · `L` lock the keyboard ·
`E` type my email · `W` `Q` vim save-and-quit

Adding a command is one line of C.

## Seven layers, all a thumb away

| Layer | Hold | What's on it |
|---|---|---|
| Nav | `Tab` or `Space R` | F1–F12, arrows, Home/End/PgUp/PgDn |
| Numbers | `Space L` | digits and everyday symbols |
| System | `Fn`, **or both spaces together** (tri layer) | media, volume, every setting |
| Code | `Tab`+`◆` | `-> => != == && \|\|`, `()` `[]` `{}` with the cursor placed inside |
| Windows | `Tab`+`✓` | Hyprland workspaces and window management |
| Git | `Tab`+`✕` | `git status/add/commit/push…` as single keys, plus recorded macros |
| Home-row mods | opt-in, persisted | GACS mods on ASDF/JKL for those who want them |

**Layer lock:** tap `'` while holding any layer and it sticks — one-handed arrows, a run of
digits — and it releases itself after a minute so the board can never feel broken.

## Typing features

- **Combos** — key pairs that never occur in English: `Q+W`→Esc, `J+K`→Esc (vim), `Z+X`→undo,
  `N+M`→Delete, `,+.`→`_`, `.+'`→`:`, `C+V`→Caps Word, `Q+P`→lock the keyboard
- **Caps Word** — caps lock that turns itself off at the end of the word; survives `_` and `-`,
  so `MAX_RETRY_COUNT` comes out whole
- **Double-tap Shift → Caps Lock** — with zero added latency on normal shifting
- **Repeat & Alt-Repeat keys** — repeat the last keystroke, or do its opposite (undo↔redo)
- **Shift+Backspace → Delete** — no reaching for a nav layer
- **Dynamic macros** — record any key sequence on the fly, two slots, replay all day
- **Key lock** — pin a key down (scrolling, games) until you press it again
- **One-shot modifiers** — tap a mod, then the key; tap twice to latch it
- **One-handed mode** — the whole board mirrors itself for coffee-in-hand moments
- **Pattern lock** — the keyboard swallows every key until you type the unlock pattern

## Under the hood

- **5,300+ matrix scans/sec**, ~0.7 ms average input latency — the USB HID floor
- Eager debounce (zero added press latency), 1000 Hz USB, LTO-optimised firmware
- All of the above in **75 KB** on a 128 KB / 16 KB RAM Cortex-M0
- Settings persist in the keyboard's own EEPROM — no software needed on any machine

And one scar worth showing off: this board silently refuses to boot any firmware image over
~83 KB of real code, looking completely bricked. Finding that took disproving a corrupted flash,
a RAM overflow, an EEPROM page collision and a bootloader write cap — the whole hunt is written
up in [`keyboard.md`](keyboard.md) so nobody has to walk it twice.
