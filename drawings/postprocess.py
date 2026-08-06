#!/usr/bin/env python3
"""Make the drawing a complete reference: add what qmk c2json cannot see.

- combos (they live in C, not the keymap array)
- the tri-layer chord (both spaces -> System)
- the Shift+Backspace -> Delete key override, as a shifted legend
- labels on the blank "held" cells

There is no Leader layer any more - that feature was removed by request.
"""
import sys, yaml

# LAYOUT_tkl_ansi key indices: row0 0-11, row1 12-22, row2 23-34, row3 35-43
COMBOS = [
    {"p": [1, 2],   "k": "Esc",       "l": ["Base"]},
    {"p": [19, 20], "k": "Esc",       "l": ["Base"]},  # vim jk
    {"p": [25, 26], "k": "Undo",      "l": ["Base"]},
    {"p": [27, 28], "k": "Caps Word", "l": ["Base"]},
    {"p": [30, 31], "k": "Del",       "l": ["Base"]},
    {"p": [31, 32], "k": "-",         "l": ["Base"]},
    {"p": [32, 33], "k": "_",         "l": ["Base"]},
    {"p": [33, 34], "k": ":",         "l": ["Base"]},
    # Q+P -> lock, and the tri layer chord: drawn as separate mini-diagrams
    {"p": [1, 10],  "k": "Lock",      "l": ["Base"], "draw_separate": True},
    {"p": [38, 40], "k": "System",    "l": ["Base"], "draw_separate": True},  # tri layer
]

# qmk c2json keeps the enum name, so TG(_WM) parses to the legend "TG( WM)".
# Draw them as what they do instead. Position -> legend, on the System layer only.
SYSTEM_TRAVEL = {
    36: {"t": "Base", "h": "drop all"},
    # "Tmux + Windows", not "WM": keymap-drawer turns a legend that matches a
    # layer name into a link to that layer's own diagram, and the abbreviation
    # missed it.
    41: {"t": "Tmux + Windows", "h": "stay"},
    42: {"t": "Spare 6", "h": "stay"},
    43: {"t": "Spare 7", "h": "stay"},
}

# The same three keys on Nav are MO(), and c2json's own legend ("layer") does not
# say the difference that matters: Nav's route dies when you let go, System's does
# not. Spell it out, so the drawing teaches the distinction on its own.
NAV_TRAVEL = {
    41: {"t": "Tmux + Windows", "h": "while held"},
    42: {"t": "Spare 6", "h": "while held"},
    43: {"t": "Spare 7", "h": "while held"},
}

doc = yaml.safe_load(sys.stdin)
doc["combos"] = COMBOS

for name, keys in doc["layers"].items():
    if name.startswith("System"):
        for pos, legend in SYSTEM_TRAVEL.items():
            keys[pos] = legend
    if name.startswith("Nav"):
        for pos, legend in NAV_TRAVEL.items():
            keys[pos] = legend
    # label the pink "you are holding this" cells instead of leaving them blank
    for i, k in enumerate(keys):
        if isinstance(k, dict) and k.get("type") == "held" and not k.get("t"):
            keys[i] = {"t": "held", "type": "held"}
    if name.startswith("Base"):
        keys[11] = {"t": "BSPC", "s": "⇧ Del"}       # key override, shifted legend
    if name.startswith("Numbers"):
        # keymap-drawer gives KC_NUBS a "|" shifted legend but KC_BSLS none, which
        # is backwards - KC_BSLS is the key that actually types the pipe. Put it back.
        keys[31] = {"t": "\\", "s": "|"}

yaml.dump(doc, sys.stdout, sort_keys=False, allow_unicode=True, width=160)
