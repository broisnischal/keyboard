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
 │ ⇧  │123 │ Z  │ X  │ C  │ V  │ B  │ N  │ M  │ ,  │ .  │ '  │
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
| **2 · Nav** | `Tab` or `Space R` | F1-F12, arrows, Home/End/PgUp/PgDn, browser back/forward |
| **3 · Symbols** | `Space L` | `` ` `` on `Esc`, then `! @ # $ % ^ & * ( )` across the top row with no `Shift`; `- = ; ' \`` `[ ] / ? \` - hold `Shift` too for `_ + : " ~ { } |` |
| **4 · System** | `Fn`, or `Space L` **+** `Space R` together | media, volume, and every setting key |
| **5 · Tmux + Windows** | `◆`, or `Tab` **+** `◆`, or **travel:** `Fn` **+** `◆` | tmux panes and windows, Hyprland workspaces |
| **6 · Digits** | the key left of `Z` (tap, hold, or double-tap to lock), or `Tab` **+** `✓` / `Fn` **+** `✓` | `1`-`0` across the top row, and the symbols `- = ; ' \` [ ] / ? \` below them |
| **7 · Spare** | `Tab` **+** `✕`, or `Fn` **+** `✕` | empty - yours to fill |

**Backtick and tilde live on `Esc`.** `Space L` + `Esc` is `` ` `` and `Space L` + `Shift` + `Esc` is
`~`, where the `` `~ `` key sits on a full-size keyboard. `Shift` + `Esc` alone also gives `~`. A bare
`Esc` is still `Esc`.

Layer 0 is plain letters. Layer 1 is an empty spare that nothing reaches - it used to hold the
home row mods, which are gone.

**Hold `Tab` for Nav still works, and `Alt`+`Tab` is now instant.** Those two used to be
incompatible: a hold-or-tap key can't send its tap until you let go, so `Alt`+`Tab` fired nothing
until your finger came up, and holding `Tab` a moment too long gave you the layer and no `Tab` at
all. The firmware now checks for a held modifier *before* it starts deciding tap-or-hold - if `Ctrl`,
`Alt`, `Shift` or `Gui` is already down, `Tab` is just `Tab`, sent on the keydown, and it repeats if
you keep holding it. `Alt`+`Tab`+`Tab`+`Tab` walks the window list normally.

The one consequence: **you can't reach Nav with `Tab` while holding a modifier** - the firmware has
decided that's a `Tab`. Use `Space R` for that, which is the other Nav key and behaves as before.

**The space bars need a short deliberate hold now - about 80 ms.** They used to give up their
layer after 130 ms, and that is shorter than an ordinary space bar press, so resting a thumb turned
a layer on by itself. The space then disappeared (a key that decided it was a hold has no tap left
to send) and the next letter came off that layer instead of the alphabet: `o` typed `9` on `Space L`
and nothing at all on `Fn` or `Space R`, and `_NAV`+`b` repeated whatever you had just typed. That
is where "some letters don't appear, some double" came from.

Three things changed, and none of them made typing slower:

- **230 ms before a space becomes a layer.** Costs nothing, because a space is sent when you *lift*
  the key, not when the timer runs out. A 90 ms space still lands at 90 ms.
- **A long space is still a space.** Hold a thumb through a pause, let go without pressing anything
  else, and you get your space. Press something during the hold and you get the layer, as before.
- **A space typed inside 110 ms of the previous letter is sent on the keydown**, so mid-sentence it
  is instant and cannot turn a layer on at all. To reach Symbols mid-word, pause for a beat first
  and then hold (or tap `123` and the key, which never has to guess) - which is what reaching for a layer feels like anyway.

Then a fourth, because the first three made the layers themselves slow to reach: 230 ms was the
only way in, so every layer chord began with a quarter-second wait, and anything typed during that
wait came out late in a burst.

- **Hold a thumb for 80 ms and the next key you press goes straight to its layer.** No waiting out
  the full term. The 230 ms above now only applies to a thumb held with nothing else pressed, which
  is the case Retro Tapping already covers by giving you the space back on release.

The 80 ms is what keeps a fast space from becoming a layer chord. Roll straight from a letter into
the space bar and the space wins; pause, then hold, and the layer wins.

Four more, 2026-10-06, after "symbols still give me spaces" and "typing is laggy":

- **A key inside the 80 ms no longer waits out 230 ms.** It used to sit in a queue until the
  thumb lifted or 230 ms passed. Now the thumb gets 50 ms after that key goes down: lift it in
  that time and it's space + letter, keep holding and it's the layer. Measured with `th40 keylog`:
  in a typing roll my thumb comes up 0-18 ms after the next key, and in a symbol chord it stays
  down 250 ms or more. Two of my symbol attempts landed at 71 and 79 ms, inside the 80 ms, and
  used to come out as "space, letter".
- **`x = 1` reaches the `=`.** A space just typed no longer counts as typing for the next thumb
  press, so tapping `Space L` and then holding it again for `=` isn't forced into a second space.
- **Both spaces reach System in either order.** `Space L` first used to type a space when
  `Space R` joined, because the Symbols layer had a plain space on that key.
- **Tuning without a flash.** `th40 keylog` prints every space bar press: time since the previous
  key, how long it was held, when the next key came, and what that resolves to. `th40 tune flow
  <ms>`, `th40 tune arm <ms>` and `th40 tune overlap <ms>` change the space bars' windows live,
  until the next replug.

`Space R` was briefly made instant the same day, on the guess that I only type spaces with the
left thumb. I type plenty with the right one, so fast typing landed on Nav (Home, arrows). Both
space bars now follow the same rules.

**Digits are on the key next to left Shift** (left of `Z`), and symbols are on `Space L`. Holding
`Shift` on top of a thumb to get `!` or `(` was the hard part, so the two swapped places:

- **Tap it**, then press a top-row key: one digit. Nothing to hold.
- **Hold it** and type a whole number.
- **Double-tap it**, or tap it then the lock key (bottom right), and it stays on - a number lock.
  `123` again, or the lock key, takes you back. While it's locked the home row gives symbols, not
  letters.

**It's also the fast way to symbols.** `- = ; ' \` [ ] / ? \` sit on the same key, in the same places
as on `Space L`. It switches on the instant you press it, while `Space L` has to guess whether you
meant a space - which is why a quick `what?` used to come out as `what l`. So `?` is: tap the key,
then `L`.

A tap forgets itself after **1.2 s**: the key sits right where your hand goes for Shift, and a stray
brush used to re-point a keystroke you made seconds later.

### Travelling to Tmux + Windows

Tap `◆` and it focuses Claude Code, as before. **Hold it and you are on Tmux + Windows** for as long
as you hold it. The routes are not equivalent:

- **Hold `◆`** - momentary, one key. Your right thumb or pinky is on `◆`, so the left hand has the
  whole tmux half and workspaces. The layer engages once `◆` has been down about 80 ms, so a quick
  tap that rolls into typing still just focuses Claude.
- **Hold `Tab` + `◆`** (or `Space R` + `◆`) - also momentary, two keys. Still works.
- **`Fn` + `◆`** - **latches.** Let go of everything and you are *on* the layer with both hands
  free, which is the only way to use both halves at once - splitting a pane and then moving the
  window.

The same press comes back: `Fn` + `◆` again turns it off. `Fn` reaches the System layer from it,
because its bottom row is pass-through. `✓` and `✕` do the same for the two spare layers.

**If the board ever seems to be typing nonsense - `Fn` + `Gui`.** That is the panic key: it drops
every latched layer at once. A latched Windows layer turns the whole alphabet into Super-chords and
looks exactly like a broken keyboard, so this key always gets you back to plain typing. Unplugging
and replugging also clears it; latched layers are never remembered.

### Layer lights

Hold or lock any layer and the board shows it: the keys that do something on that layer glow in
its colour, the key you're holding glows white, and everything else goes dark. **Nav is blue,
Symbols orange, System violet, Tmux + Windows green, Digits yellow.** A latched or locked layer
keeps glowing, so a stuck layer is obvious at a glance. It works with the backlight off too.
`Fn` + `T` turns it off and on, and the keyboard remembers.

### Locking a layer

The bottom-right key (the right-Shift spot, `'` on Base) is the **lock key** on Nav, Symbols, System
and Digits, and it follows one rule:

- **Holding a layer? Tap it and the layer stays** after you let go. With Digits you don't even need
  to hold: tap `123`, then the lock key.
- **Holding nothing? Tap it and you're back on Base**, whatever was keeping you on the layer - a
  lock, a double-tapped `123`, or an `Fn` travel.

So: hold, lock, type, lock key again, home. The layer lights stay on the whole time it's locked.
`123` pressed while Digits is locked also takes you home. Safety net: a locked layer releases
itself after a minute of no typing.

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
| `Q` + `P` | **lock the keyboard** (see below) |

**Combos switch themselves off while you're typing.** A key that belongs to a combo normally can't be
sent the instant you press it - the firmware waits up to 40 ms to see whether its partner is coming.
Mid-word that wait is pure lag, so a combo key pressed within 120 ms of the previous key just types,
instantly. A combo fires when you start it after a short pause, which is how you reach for one
anyway.

The exceptions are the combos you use mid-word: `_`, `-`, `:` and `J`+`K`. Those always work, so
`M , . ' J K` still wait. `Q W Z X C V N P` are instant while you type. If a combo you meant comes
out as two letters, pause a beat first, or lower `COMBO_FLOW_TERM` in `keymap/config.h`.

### The prefix key - removed

Double-tapping left `Ctrl` used to arm a tmux-style prefix. You asked for it gone, and removing it
fixed something else at the same time: making a double-tap possible meant `Ctrl` had to be a
*tap-dance* key with a 200 ms decision window, and that is what made `Ctrl`+`A` and every other
`Ctrl` shortcut feel a beat late. `Ctrl` is now an ordinary `Ctrl`.

The tmux commands it used to send are real keys now, on the Tmux + Windows layer below - and they
send tmux's own `Ctrl-b` sequences, so there's no waiting for a timeout at all.

### Backspace always deletes backward

`Shift`+`Backspace` and `Space R`+`Backspace` both used to be Delete. Both bit mid-correction:
`Shift` is still down from the capital you just typed, and `Space R` is still down from the space,
so the fix deleted the character *after* the cursor. Backspace is now Backspace on every layer
except one deliberate chord: **Delete is `Fn` + `Backspace`**, or `N`+`M` together. `Fn` is the small
middle key - nothing types a space with it, so it is never still down from the last word.

### Recorded macros

Record any key sequence on the fly and replay it - two slots, held in memory until unplugged.
They moved here from the old Git layer.

| Key (hold `Fn`) | Does |
|---|---|
| `B` / `M` | start recording macro 1 / 2 |
| `.` | stop recording |
| `N` / `,` | play macro 1 / 2 |

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

### Home row mods - removed

Gone, not just switched off: holding a letter never gives a modifier. `Fn` + `T`, which used to
switch them on, does nothing now.

### Keyboard lock

**What it is:** the keyboard ignores everything until you type a pattern.

**Use it:** `Q`+`P` together, or `Fn` + `A`. The board goes dead.

**To unlock:** press the **top-left key**, then type **N → E → E → S** within 5 seconds. Any wrong
key cancels and re-locks.

Two things worth knowing. It does **not** lock automatically - not at boot, not on idle - unless you
turn that on with `Fn` + `Q`. And it isn't security: the pattern is in the firmware source, and
anyone who can unplug the board can reflash it. It stops someone walking past your desk.

---

## Layer 5 · Tmux + Windows

One layer, split down the middle: **left hand talks to tmux, right hand moves windows.** The old
Code and Git layers are gone; this replaced both.

The tmux half sends tmux's real `Ctrl-b` sequences, so tmux needs no configuration. The window half
sends the `Super` chords omarchy already binds, so Hyprland needs none either.

| Key | Does |
|---|---|
| `Q` to `P` | **workspace 1-10** |
| `A` | tmux: new window |
| `S` `D` | tmux: split side-by-side · split stacked |
| `F` `G` | tmux: zoom the pane · kill the pane |
| `H` `J` `K` `L` | tmux: **focus pane** left / down / up / right |
| `/` | tmux: back to the last window |
| `Z` `X` | tmux: previous · next window |
| `C` `V` | tmux: choose a window · choose a session |
| `B` | tmux: detach |
| `N` `M` `,` `.` | **focus window** left / down / up / right |
| `'` | close the window |
| `Gui` | panic - drop every latched layer |

The two nav rows line up on purpose: `H J K L` moves you between tmux **panes**, and `N M , .`
directly below moves you between **windows on the desktop**. Same shape, one level out.

Workspaces get the whole top row because on a 40% they're otherwise unreachable - `Super`+`3` on the
base layer would mean holding `Gui` *and* the Digits key *and* `E`.

**If your tmux prefix isn't `Ctrl-b`,** it's one line: `TMUX_PFX` at the top of the macro table in
`keymap/keymap.c`. Every sequence follows from it.

## Layer 6 · Digits

`1`-`0` on the top row, the Symbols-layer symbols on the rows below, thumbs fall through. Reach it with the key left of
`Z` (see *Getting to layers*), or `Tab` + `✓` while held, or `Fn` + `✓` to latch it.

One catch while it's locked: it sits above Nav, Symbols and System, so their top two rows give
digits and symbols too (`Fn` + `Q` is `1`, not a setting). Release Digits first. `Fn` + `Gui` drops it like any other
latched layer.

## Layer 7 · Spare

Empty and pass-through - reaching it changes nothing. It exists so there's somewhere to put the
next idea without renumbering anything (layer numbers are load-bearing on this board). Fill it in
VIA as layer 7, or in `keymap.c`.

---

## Layer 4 · System and settings

Everything here persists to the keyboard's own memory and survives unplugging.

| Key | Does |
|---|---|
| `Q` | require the unlock pattern at power-up (off by default) |
| `W` | turn the Claude lamps on/off |
| `E` / `R` | lamp brightness down / up |
| `T` | layer lights on/off |
| `Y` | **Aura** effect - board mirrors the lamps |
| `U` | **Rain** effect - Matrix rain, seeded by your typing |
| `I` | backlight off (keys dark, lamps stay alive) |
| `Backspace` | Delete (forward) |
| `A` | lock the keyboard |
| `S` | one-handed mode |
| `D` | Caps Word |
| `F` | Repeat |
| `G` to `L` | prev / play / next / vol down / vol up |
| `/` `Z` `X` `C` | one-shot Shift / Ctrl / Alt / Gui |
| `V` | key lock - pin the next key down |
| `B` `N` `M` `,` `.` | macro: record 1 · play 1 · record 2 · play 2 · stop |
| `◆` `✓` `✕` | travel to Tmux+Windows / Digits / Spare 7 and stay there |
| `Gui` | panic - drop every latched layer |

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
| `◆` | tap: focus the terminal running Claude Code, or launch it · hold: Tmux + Windows |
| `✓` | send Enter to it - approve a permission prompt |
| `✕` | send Escape to it - interrupt |

`✓` and `✕` find the window themselves, so they work even while you're in the browser.

---

## Using the lamps for your own things

The lamps are a four-slot priority bus. Slot 0 is Claude; slots 1-3 are yours. The highest-priority
live slot wins. (Slot 3 is fully yours now - the prefix key used to borrow it for its cyan lamp.)

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

**A layer key does nothing.** Tmux+Windows needs `Tab` (or `Space R`) held *first*, then `◆`. If
you're also holding a modifier, `Tab` won't reach Nav at all - that's deliberate, so `Alt`+`Tab`
works; use `Space R` instead.

**Keyboard completely dead.** You may have locked it - press the top-left key, then type `N E E S`.

**Typing feels laggy.** The one place a keypress waits is a combo key. Mid-word only
`M , . ' J K` still wait (up to 40 ms); `Q W Z X C V N P` skip it - see *Combos* above for the knobs. `Tab` keeps its Nav hold without any wait, because a held modifier
bypasses the tap-hold decision entirely.

**A letter doesn't appear, or a character doubles.** Almost always an accidental layer: a space bar
held long enough to become one, or the one-shot `123` key brushed. Both are much harder to trigger
since the space bars went to a 230 ms hold, so if it is still happening, measure before assuming a
firmware bug:

```bash
./chatter-watch.py     # type a sentence, then Enter
```

It reads the keyboard's own event stream, so it shows what the **keyboard** sent rather than what
the window drew. Two things to look for. Anything in `<angle brackets>` is not a letter - `<F9>`
where you typed `o` means a space layer was on. And any key listed as re-pressed under 40 ms after
its own release is a chattering switch, which no firmware change fixes: reseat or swap it and raise
`DEBOUNCE` in `keymap/config.h`. If the text looks right and nothing is listed, the board is fine.

**A tmux key does nothing.** Your prefix isn't `Ctrl-b`. Change `TMUX_PFX` in `keymap/keymap.c` and
reflash.

**Wrong letters after using one-handed mode.** It's still on. `Fn` + `S`.

**Stuck on a layer.** Let go of everything and tap the bottom-right key: that always goes back to
Base. Or wait a minute and a lock releases itself.

**A key seems held down forever.** Key lock is pinning it - press that key once to release it.
