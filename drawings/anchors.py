#!/usr/bin/env python3
"""Print the base keycap in the corner of every cell that isn't the base.

A layer diagram on its own tells me what a key does, never which key it is. `[`
on the Numbers layer is the eighth cell of the second row, and finding it means
counting columns across to the Base diagram. So each overlay cell gets the Base
legend for that position in its top-left corner: `[` reads "H", and I press the
key my finger already knows.

keymap-drawer has no field for a fourth legend - `t`, `h` and `s` are the three,
and `s` is taken on the Numbers backslash - so this runs on the finished SVG.
Each cell is a `<g class="key keypos-N">` inside a `<g class="layer-NAME">`, so
the position is addressable and the injection is a text node after the rect.

Usage: anchors.py keymap.yaml < keymap.svg > keymap-anchored.svg
"""
import re
import sys
from html import escape

import yaml

# Anchors are 8px in a corner. Base legends that are words get an initial or a
# glyph; anything not listed is already short enough to print as it is.
SHORT = {
    "BSPC": "⌫",
    "ENT": "⏎",
    "Space": "␣",
    "Ctrl": "Ctl",
    "LGUI": "Gui",
    "LALT": "Alt",
    "Num": "Num",
    "Tab": "Tab",
    "Esc": "Esc",
}

KEY_RE = re.compile(
    r'(<g transform="[^"]*" class="key keypos-(\d+)">\n'
    r'<rect [^>]*?x="(-?[\d.]+)" y="(-?[\d.]+)"[^>]*/>\n)'
)


def tap(key):
    """The tap legend of a parsed key, whatever shape the entry has."""
    if isinstance(key, str):
        return key
    if isinstance(key, dict):
        return str(key.get("t", ""))
    return ""


def main():
    doc = yaml.safe_load(open(sys.argv[1]))
    layers = doc["layers"]
    base = [tap(k) for k in next(iter(layers.values()))]

    svg = sys.stdin.read()
    # Split on layer groups; chunk 0 is the header, then (open tag, body) pairs.
    parts = re.split(r'(<g transform="[^"]*" class="layer-([^"]*)">)', svg)
    out = [parts[0]]

    for i in range(1, len(parts), 3):
        open_tag, name, body = parts[i], parts[i + 1], parts[i + 2]
        keys = layers.get(name)
        # Skip the base diagram (it *is* the anchor) and the combo mini-boards.
        if keys is None or name == next(iter(layers)):
            out += [open_tag, body]
            continue

        def anchor(m):
            pos = int(m.group(2))
            if pos >= len(keys) or pos >= len(base):
                return m.group(1)
            legend = tap(keys[pos])
            cap = base[pos]
            # Nothing to anchor: transparent cells fall through to Base anyway,
            # and a cell that already prints the base legend is its own anchor.
            if not cap or not legend or legend == cap or legend == "▽":
                return m.group(1)
            x = float(m.group(3)) + 5
            y = float(m.group(4)) + 7
            text = escape(SHORT.get(cap, cap))
            return f'{m.group(1)}<text x="{x:g}" y="{y:g}" class="key anchor">{text}</text>\n'

        out += [open_tag, KEY_RE.sub(anchor, body)]

    sys.stdout.write("".join(out))


if __name__ == "__main__":
    main()
