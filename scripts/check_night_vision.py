#!/usr/bin/env python3
"""Check that a captured astroterm screen emits only reds on black.

Reads the output of `tmux capture-pane -e -p` on stdin, tracks SGR color state
cell by cell, and fails if any visible character is drawn with a foreground
that is not red, a background that is not black, or is an emoji (which
terminals draw in full color whatever the SGR state).

    tmux capture-pane -e -p -t <pane> | scripts/check_night_vision.py
"""

import re
import sys

RED_FG = {"31", "91", "38;5;196", "38;5;160", "38;5;88", "38;5;1", "38;5;9"}
BLACK_BG = {"40", "48;5;16", "48;5;0"}

SGR = re.compile(r"\x1b\[([0-9;]*)m")


def parse_sgr(params, state):
    codes = params.split(";") if params else ["0"]
    i = 0
    while i < len(codes):
        c = codes[i] or "0"
        if c == "0":
            state["fg"], state["bg"] = "39", "49"
        elif c in ("38", "48") and i + 2 < len(codes) and codes[i + 1] == "5":
            state["fg" if c == "38" else "bg"] = f"{c};5;{codes[i + 2]}"
            i += 2
        elif c in ("38", "48") and i + 4 < len(codes) and codes[i + 1] == "2":
            state["fg" if c == "38" else "bg"] = f"{c};2;" + ";".join(codes[i + 2 : i + 5])
            i += 4
        elif 30 <= int(c) <= 37 or 90 <= int(c) <= 97 or c == "39":
            state["fg"] = c
        elif 40 <= int(c) <= 47 or 100 <= int(c) <= 107 or c == "49":
            state["bg"] = c
        i += 1


def main():
    text = sys.stdin.read()
    state = {"fg": "39", "bg": "49"}
    problems = []
    visible = 0
    def check(row, chunk):
        nonlocal visible
        for ch in chunk:
            if ch == " ":
                if state["bg"] not in BLACK_BG:
                    problems.append((row, repr(ch), state["fg"], state["bg"]))
                continue
            visible += 1
            emoji = 0x1F000 <= ord(ch) <= 0x1FAFF
            if emoji or state["fg"] not in RED_FG or state["bg"] not in BLACK_BG:
                problems.append((row, ch, state["fg"], state["bg"]))

    # tmux carries SGR state across line breaks, and so does this parser
    for row, line in enumerate(text.split("\n")):
        pos = 0
        for m in SGR.finditer(line):
            check(row, line[pos : m.start()])
            parse_sgr(m.group(1), state)
            pos = m.end()
        check(row, line[pos:])

    if problems:
        print(f"FAIL: {len(problems)} cells are not red on black (of {visible} visible glyphs)")
        for p in problems[:15]:
            print(f"  row {p[0]}: {p[1]} fg={p[2]} bg={p[3]}")
        sys.exit(1)
    print(f"OK: {visible} visible glyphs, all red on black")


if __name__ == "__main__":
    main()
