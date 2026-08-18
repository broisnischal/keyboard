#!/usr/bin/env python3
"""Show what the TH40 actually sent, and flag keystrokes no finger could produce.

Reads the keyboard's own evdev node, so it reports what the KEYBOARD emitted rather
than what the focused window chose to render. Two things it settles that guesswork
never will:

  * Anything that is not a letter, digit or space prints in <ANGLE BRACKETS>. That
    is exactly what an accidentally engaged space layer looks like: <F9> where an
    "o" should be, a doubled letter where QK_REP fired, a missing space where a
    hold swallowed one.
  * A key re-pressed sooner than CHATTER_MS after its own release is switch
    chatter. No firmware change fixes that, so it has to be ruled out before
    blaming the keymap.

    ./chatter-watch.py            # runs 600s, or until Enter
    ./chatter-watch.py 60         # shorter window

Reads /dev/input directly, so it sees keys even when the focused window swallows
them. Needs membership of the `input` group (I have it) or sudo.

Uses os.open + select, not open()/read(): a buffered reader on a non-blocking fd
returns None unpredictably and silently captures nothing, which is indistinguishable
from "nothing was typed". That cost me one wasted capture.
"""
import os
import select
import struct
import sys
import time

DEV = "/dev/input/by-id/usb-EPOMAKER_EPOMAKER_TH40_00000000000000000000000000000000-event-kbd"

# Below this gap between one release and the next press of the same key, a finger
# has not had time to travel. 40 ms is generous - the fastest deliberate double
# taps measure around 70 ms.
CHATTER_MS = 40

EV_KEY = 0x01
FMT = "llHHi"  # input_event: sec, usec, type, code, value
SIZE = struct.calcsize(FMT)

PLAIN = {
    2: "1", 3: "2", 4: "3", 5: "4", 6: "5", 7: "6", 8: "7", 9: "8", 10: "9", 11: "0",
    12: "-", 13: "=", 16: "q", 17: "w", 18: "e", 19: "r", 20: "t", 21: "y", 22: "u",
    23: "i", 24: "o", 25: "p", 26: "[", 27: "]", 30: "a", 31: "s", 32: "d", 33: "f",
    34: "g", 35: "h", 36: "j", 37: "k", 38: "l", 39: ";", 40: "'", 41: "`",
    43: "\\", 44: "z", 45: "x", 46: "c", 47: "v", 48: "b", 49: "n", 50: "m",
    51: ",", 52: ".", 53: "/", 57: " ",
}
SPECIAL = {
    1: "ESC", 14: "BSPC", 15: "TAB", 28: "ENT", 58: "CAPS", 99: "PRTSC",
    102: "HOME", 107: "END", 104: "PGUP", 109: "PGDN", 110: "INS", 111: "DEL",
    103: "UP", 108: "DOWN", 105: "LEFT", 106: "RIGHT",
}
MODS = {29: "Ctrl", 97: "Ctrl", 42: "Shift", 54: "Shift", 56: "Alt", 100: "AltGr", 125: "Gui"}
for _i in range(12):
    SPECIAL[59 + _i] = f"F{_i + 1}"
for _i, _code in enumerate(range(183, 195)):
    SPECIAL[_code] = f"F{_i + 13}"


def label(code):
    return PLAIN.get(code) or SPECIAL.get(code) or f"code{code}"


def main():
    timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 600.0
    if not os.path.exists(DEV):
        sys.exit(f"{DEV} is gone - is the keyboard plugged in?")

    fd = os.open(DEV, os.O_RDONLY | os.O_NONBLOCK)
    poller = select.poll()
    poller.register(fd, select.POLLIN)

    out = []
    chatter = []
    released_at = {}
    held_mods = set()
    deadline = time.monotonic() + timeout
    print(f"watching {os.path.realpath(DEV)} for up to {int(timeout)}s")
    print("type normally; Enter ends the run\n", flush=True)

    done = False
    while not done and time.monotonic() < deadline:
        if not poller.poll(500):
            continue
        try:
            buf = os.read(fd, SIZE * 64)
        except BlockingIOError:
            continue
        for off in range(0, len(buf) - SIZE + 1, SIZE):
            sec, usec, etype, code, value = struct.unpack(FMT, buf[off:off + SIZE])
            if etype != EV_KEY or value == 2:  # skip autorepeat
                continue
            ms = sec * 1000 + usec // 1000

            if value == 0:
                released_at[code] = ms
                held_mods.discard(code)
                continue

            if code in MODS:
                held_mods.add(code)
                continue

            prev = released_at.get(code)
            if prev is not None and ms - prev < CHATTER_MS:
                chatter.append((code, ms - prev))

            if code == 28:  # Enter ends the run
                done = True
                break

            pfx = "".join(f"{MODS[m]}+" for m in sorted(held_mods))
            if code in PLAIN and not pfx:
                out.append(PLAIN[code])
            else:
                out.append(f"<{pfx}{label(code)}>")
            print("".join(out), flush=True)

    os.close(fd)
    print("\n=== KEYBOARD SENT ===")
    print("".join(out) or "(nothing at all - no key events reached this script)")
    print(f"\n{len(out)} keystrokes")

    if chatter:
        print("\nCHATTERING SWITCHES - a key re-pressed sooner than a finger can move:")
        for code, gap in chatter:
            print(f"  {label(code):>5}  re-pressed {gap} ms after its own release")
        print("\nThis is hardware. Reseat or swap those switches, and raise DEBOUNCE")
        print("in keymap/config.h (7 -> 12) before looking at the keymap.")
    elif out:
        print("\nno chatter: nothing was re-pressed within 40 ms of its own release.")
        print("Any doubled character above came from the firmware, not the switches -")
        print("look for QK_REP, QK_LOCK or DM_PLY* reachable by an accidental layer.")


if __name__ == "__main__":
    main()
