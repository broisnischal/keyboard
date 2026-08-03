#!/usr/bin/env python3
"""Make the drawing a complete reference: add what qmk c2json cannot see.

- combos (they live in C, not the keymap array)
- the tri-layer chord (both spaces -> System)
- the Shift+Backspace -> Delete key override, as a shifted legend
- the Leader layer: sequences from leader_end_user(), drawn on their keys
- labels on the blank "held" cells
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

# leader_end_user() in keymap.c, one entry per sequence
LEADER_KEYS = {
    35: {"t": "Ctrl ×2", "type": "held"},           # the trigger itself
    14: {"t": "Sleep", "h": "system"},              # S
    10: {"t": "Play", "h": "pause"},                # P
    30: {"t": "Next", "h": "track"},                # N
    29: {"t": "Prev", "h": "track"},                # B
    31: {"t": "Mute"},                              # M
    21: {"t": "Lock", "h": "keyboard"},             # L
    3:  {"t": "Email", "h": "types it"},            # E
    2:  {"t": ":wq", "h": "then Q"},                # W
    1:  {"t": ":wq", "h": "after W"},               # Q
}

doc = yaml.safe_load(sys.stdin)
doc["combos"] = COMBOS

for name, keys in doc["layers"].items():
    # label the pink "you are holding this" cells instead of leaving them blank
    for i, k in enumerate(keys):
        if isinstance(k, dict) and k.get("type") == "held" and not k.get("t"):
            keys[i] = {"t": "held", "type": "held"}
    if name.startswith("Base"):
        keys[11] = {"t": "BSPC", "s": "⇧ Del"}       # key override, shifted legend
    if name.startswith("Leader"):
        for pos, legend in LEADER_KEYS.items():
            keys[pos] = legend

yaml.dump(doc, sys.stdout, sort_keys=False, allow_unicode=True, width=160)
