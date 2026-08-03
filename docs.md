# TH40 - how to use it

A user guide, not a build guide. For firmware internals, flashing and recovery see
[`keyboard.md`](keyboard.md).

---

## The board

44 keys. Everything else is layers.

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

The last three keys are the **Claude keys**. `Fn` is the small key between the two spacebars.

---

## Getting to layers

Every layer is a **hold**. Nothing latches unless you ask it to.

| Layer | Hold | Contains |
|---|---|---|
| **1 · Nav** | `Tab` or `Space R` | F1-F12, arrows, Home/End/PgUp/PgDn, browser back/forward |
| **2 · Numbers** | `Space L` | digits, `- = ; ' \`` `[ ] / ?` |
| **3 · System** | `Fn`, or `Space L` **+** `Space R` together | media, volume, and every setting key |
| **4 · Code** | `Tab` **+** `◆` | operators and digraphs |
| **5 · Windows** | `Tab` **+** `✓` | Hyprland workspaces and windows |
| **6 · Git** | `Tab` **+** `✕` | git and shell one-liners |

The `/` key next to left Shift is a **one-shot** for layer 2: tap it, then press one key, and you get
that key's layer-2 meaning without holding anything. Tap it again to cancel.

Layers 4-6 are reached by holding `Tab` first, then holding one of the bottom-right keys - the same
three keys that are the Claude keys on the base layer.

### Locking a layer

While holding a layer, tap the `'` key (bottom-right corner) and the layer **stays** when you let
go - useful for one-handed arrow work or a run of digits. Tap the same key again to release it.
Safety net: a locked layer releases itself after a minute of no typing, so it can never leave the
board feeling broken. Works on Nav, Numbers, System and Git.

---

## Typing features

### Double-tap Shift → Caps Lock

Hold Shift, it's Shift. Tap it twice quickly, Caps Lock toggles. Shift still registers the instant
you press it, so this costs you nothing while typing.

### Caps Word

**What it is:** Caps Lock that switches itself off at the end of the word. For typing one constant
without reaching for Caps Lock twice.

**Use it:** press `C`+`V` together, or `Fn` + `D`. Then type. It ends on space, punctuation, or
after 5 seconds idle.

```
C+V  then  max_retry_count   →   MAX_RETRY_COUNT
```

It deliberately stays alive through `-` and `_`, so `SCREAMING_SNAKE_CASE` and `CONST-NAMES` come
out whole.

### Repeat key

**What it is:** one key that repeats whatever you just typed - letter, word-part, or shortcut.

**Use it:** `Tab` + `B`. Or `Fn` + `F`.

```
type  k      then Repeat ×4   →   kkkkk
Ctrl+Z       then Repeat ×3   →   four undos
```

There's also **Alt-Repeat** on `Tab` + `N`, which does the *opposite* of the last key where one
exists - after `Ctrl+Z` it redoes, after `Page Down` it pages up, after `→` it goes left.

### Combos

Press both keys at the same time. Each pair is a letter combination that essentially never occurs in
English, so fast typing can't trigger them by accident.

| Press together | Get |
|---|---|
| `Q` + `W` | `Esc` |
| `Z` + `X` | Undo (`Ctrl+Z`) |
| `C` + `V` | Caps Word |
| `N` + `M` | `Delete` |
| `M` + `,` | `-` |
| `,` + `.` | `_` |
| `J` + `K` | `Esc` (the vim escape, on the home row) |
| `.` + `'` | `:` (otherwise needs a layer plus shift) |
| `F` + `J` | **arm the prefix key** (see below) |
| `Q` + `P` | **lock the keyboard** (see below) |

### The prefix key - like tmux's Ctrl-b

Press `F`+`J` together (both index fingers, home position) and the three lamps turn **cyan**: the
keyboard is armed and waiting for a command, exactly like tmux after its prefix. Then one short
sequence runs an action:

| Then press | Does |
|---|---|
| `S` | put the computer to sleep |
| `P` | music: pause / play |
| `N` | music: next track |
| `B` | music: back a track |
| `M` | mute |
| `L` | lock the keyboard |
| `E` | type your email address |
| `W` `Q` | vim: Esc `:wq` Enter - save and quit |

There's no rush - the armed prefix waits for you (the cyan lamp shows it's live). Once you start a
sequence, the action fires about a third of a second after the last key. A key that matches
nothing just fizzles, cyan goes out, nothing is typed.

The sequences live in one obvious table in `keymap.c` (`leader_end_user`) - adding one is a single
line.

### Shift+Backspace = Delete

Hold Shift and press Backspace to delete forward. Plain Backspace is unchanged.

### Recorded macros

Record any key sequence on the fly and replay it - two slots, held in memory until unplugged.

| Key (on the Git layer, `Tab` + `✕`) | Does |
|---|---|
| `H` / `K` | start recording macro 1 / 2 |
| `N` | stop recording |
| `J` / `L` | play macro 1 / 2 |

Record repetitive edits once, replay them all day. The slots are RAM only - a reboot clears them.

### Key lock - hold a key without holding it

`Fn` + `V`, then press any key: it stays held down until you press it again. For scrolling long
pages, walking in games, or anything that wants a key pinned while your hands do something else.

### One-shot modifiers

**What it is:** tap a modifier, then tap a key - instead of holding both. Easier on the hands, and
useful for one-finger use.

**Use it:** `Fn` + one of `/` `Z` `X` `C` = one-shot Shift, Ctrl, Alt, Gui.

```
Fn+/  release everything,  then a    →   A
```

Tap the same one-shot **twice** and it locks on until you tap it a third time.

### One-handed mode (Swap Hands)

**What it is:** the keyboard mirrors itself. Press the key where `J` is and you get `F`; the whole
right half becomes a mirror image of the left. It means you can reach every key on the board with
one hand - useful when your other hand is holding a coffee, a phone, or a cable.

**Use it:** `Fn` + `S` toggles it. Press again to go back.

```
normal:   A S D F  G   H  J K L
mirrored: L K J H  G   F  D S A
```

Muscle memory is the catch - your fingers know where letters are, and half of them have moved. It's
a tool for occasional one-handed stretches, not a way to type all day.

### Home row mods - off by default

**What it is:** holding `A` gives Super, `S` Alt, `D` Ctrl, `F` Shift, mirrored on `J K L`. Tapping
them still types letters. Your hands never leave home position to reach a modifier.

**They're switched off** because they made typing feel laggy - a mod-tap can't emit the letter until
it knows whether you're tapping or holding, so it fires on release rather than press.

**To try them again:** `Fn` + `T`. It swaps the base layer and remembers across reboots. Same key
switches back. Chordal Hold and Flow Tap are configured, so same-hand rolls type letters normally
and holds are suppressed entirely while you're mid-flow - but the first key after a pause still
resolves late, and that's inherent to the technique.

### Keyboard lock

**What it is:** the keyboard ignores everything until you type a pattern.

**Use it:** `Q`+`P` together, or `Fn` + `A`. The board goes dead.

**To unlock:** press the **top-left key**, then type **N → E → E → S** within 5 seconds. Any wrong
key cancels and re-locks.

Two things worth knowing. It does **not** lock automatically - not at boot, not on idle - unless you
turn that on with `Fn` + `Q`. And it isn't security: the pattern is in the firmware source, and
anyone who can unplug the board can reflash it. It stops someone walking past your desk.

---

## Layer 4 · Code

Number row gives `! @ # $ % ^ & * - +`.

Home row is the things you type constantly:

| Key | Types |
|---|---|
| `A` `S` `D` `F` | `->` `=>` `!=` `==` |
| `G` `H` | `&&` `\|\|` |
| `J` `K` `L` | `()` `[]` `{}` - **both halves, cursor placed between them** |

Bottom row: `~` `` ` `` `\` `|` `<` `>`, then `<=` `>=` `::` and `""`.

## Layer 5 · Windows

Sends the chords omarchy already binds, so there's nothing to configure.

| Row | Does |
|---|---|
| `Q`…`P` | switch to workspace 1-10 |
| `A`…`L` | move the current window to workspace 1-9 |
| `Z` `X` | close window, fullscreen |
| `C` `V` `B` `N` | focus left / down / up / right |

## Layer 6 · Git and shell

| Key | Runs |
|---|---|
| `Q` `W` `E` | `git status` · `git add -A` · `git commit -m "` |
| `R` `T` | `git push` · `git pull` |
| `Y` `U` | `git log --oneline -15` · `git diff` |
| `I` `O` `P` | `git checkout ` · `git branch` · `git stash` |
| `A` `S` | `claude` · `claude --continue` |
| `D` `F` `G` | `lazygit` · `cd ..` · `ls -la` |
| `H` `J` `K` `L` `N` | macro: record 1 · play 1 · record 2 · play 2 · stop |

**These are guesses at your workflow.** They live in one table at the top of
`qmk/keyboards/epomaker/th40/keymaps/tapdance/keymap.c` - changing a command is a one-line edit.

---

## Layer 3 · System and settings

Everything here persists to the keyboard's own memory and survives unplugging.

| Key | Does |
|---|---|
| `Q` | require the unlock pattern at power-up (off by default) |
| `W` | turn the Claude lamps on/off |
| `E` / `R` | lamp brightness down / up |
| `T` | switch the base layer to the home-row-mod version |
| `Y` | **Aura** effect - board mirrors the lamps |
| `U` | **Rain** effect - Matrix rain, seeded by your typing |
| `I` | backlight off (keys dark, lamps stay alive) |
| `A` | lock the keyboard |
| `S` | one-handed mode |
| `D` | Caps Word |
| `F` | Repeat |
| `G`…`L` | prev / play / next / vol− / vol+ |
| `/` `Z` `X` `C` | one-shot Shift / Ctrl / Alt / Gui |
| `V` | key lock - pin the next key down |

The play key (`H`) is earbud-style: tap for play/pause, double-tap for next track, triple-tap for
previous.

Settings reset to defaults after a firmware update - that's the same mechanism that reloads your
keymap, so it's expected.

---

## The three lamps

The lights above the keys mirror what Claude Code is doing.

| Lamps | Means |
|---|---|
| Dim green, slow breath | waiting for your prompt |
| Amber pulse | thinking |
| Amber chase, left to right | running a tool |
| **Red blink** | **needs your approval** |
| Green triple-flash | finished - settles back to green breath |
| Red solid | something failed |

Red is reserved. Nothing routine is red, so red in the corner of your eye is always worth looking at.

With the **Aura** effect on (`Fn` + `Y`), the whole keyboard does the same animation at the same
instant - the board is split into thirds and each third follows the lamp above it.

### The Claude keys

| Key | Does |
|---|---|
| `◆` | focus the terminal running Claude Code, or launch it |
| `✓` | send Enter to it - approve a permission prompt |
| `✕` | send Escape to it - interrupt |

`✓` and `✕` find the window themselves, so they work even while you're in the browser.

---

## Using the lamps for your own things

The lamps are a four-slot priority bus. Slot 0 is Claude; slots 1-3 are yours. The highest-priority
live slot wins. (The prefix key borrows slot 3 for its cyan lamp while armed - anything you put
there is shadowed only for that moment.)

```bash
th40 bus 1 blink red --ttl 60 --priority 95     # urgent, clears itself after 60s
th40 bus 2 breath amber --priority 15           # quiet background state
th40 clear 2
```

Patterns: `off solid breath pulse chase blink strobe`
Colours: `red green amber orange yellow cyan blue violet pink`, or `#rrggbb`

**Always give alerts a TTL.** The keyboard expires them itself, so a script that dies can't leave
your lamps stuck on.

Some things worth wiring up:

```bash
# disk filling up
[ "$(df --output=pcent / | tail -1 | tr -dc 0-9)" -gt 90 ] \
  && th40 bus 1 breath red --ttl 300 --priority 80

# no network
ping -c1 -W2 1.1.1.1 >/dev/null 2>&1 || th40 bus 2 blink amber --ttl 120 --priority 70

# long build finished
make && th40 bus 3 strobe green --ttl 10 --priority 60 \
      || th40 bus 3 solid red --ttl 30 --priority 90
```

Other commands:

```bash
th40 status working      # drive slot 0 by hand
th40 selftest            # walk every pattern
th40 scan-rate           # matrix scans/second
th40 --help
```

---

## When something seems wrong

**Lamps dark.** Check the backlight isn't off in a way that predates the fix - press `Fn` + `Y` to
select Aura. If they're still dark, `th40 selftest` will tell you whether the keyboard is receiving
anything at all.

**Lamps stuck on a colour.** A script set a bus slot without a TTL. `th40 clear 1` (and 2, 3).

**Lamps don't follow Claude Code.** The hooks live in a plugin that loads at session start - restart
Claude Code. `th40 status working` should still work by hand regardless.

**A layer key does nothing.** Layers 4-6 need `Tab` held *first*, then the bottom-right key.

**Keyboard completely dead.** You may have locked it - press the top-left key, then type `N E E S`.

**Typing feels laggy.** Check you haven't switched to the home-row-mod layer with `Fn` + `T`. Press
it again to go back.

**Wrong letters after using one-handed mode.** It's still on. `Fn` + `S`.

**Stuck on a layer.** You locked it - tap the `'` key, or just wait a minute and it releases
itself.

**A key seems held down forever.** Key lock is pinning it - press that key once to release it.
