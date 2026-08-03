#!/usr/bin/env python3
"""Add the combos that qmk c2json cannot see (they live in C, not the keymap array)."""
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
    {"p": [16, 19], "k": "Leader",    "l": ["Base"], "draw_separate": True},  # F+J prefix
    {"p": [1, 10],  "k": "Lock",      "l": ["Base"], "draw_separate": True},
]

doc = yaml.safe_load(sys.stdin)
doc["combos"] = COMBOS
yaml.dump(doc, sys.stdout, sort_keys=False, allow_unicode=True, width=160)
