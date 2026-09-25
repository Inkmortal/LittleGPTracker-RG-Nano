#!/usr/bin/env python3
"""Turn the user guide (docs/rgnano-wiki/*.md) into the app's built-in guide.

Writes projects/resources/guide/guide.txt, which ships next to the binary
(OPK and simulator) and is read by the in-app Guide (Help button on the
start screen, A in the RB+Select helper). Run it whenever the wiki changes;
tools/install-rgnano.ps1 and the CI build run it too.

Format (one entry per line, first character is the kind):
    @<id>|<title>    a page (topic)
    =<title>         a section inside the page (Left/Right jump between them)
    ' '<text>        body text, already wrapped to WIDTH columns
    '|'<text>        code / table text, drawn in the accent color
    '*'<text>        a tip or table key, drawn highlighted

Usage:
    python tools/build_ingame_guide.py [--check]
"""

from __future__ import annotations

import argparse
import re
import sys
import textwrap
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
WIKI = ROOT / "docs" / "rgnano-wiki"
OUT = ROOT / "projects" / "resources" / "guide" / "guide.txt"

WIDTH = 26  # characters inside the guide window on a 240x240 screen

# Order follows the wiki sidebar, then the overview and the developer notes,
# so every page of the web guide is also in the app
PAGES = [
    ("Your-First-Song", "first-song"),
    ("Controls", "controls"),
    ("How-Trackers-Work", "trackers"),
    ("Screens", "screens"),
    ("Synth", "synth"),
    ("Macro-Synth", "macro"),
    ("Commands", "commands"),
    ("Music-Theory-Cheat-Sheet", "theory"),
    ("Demo-Songs", "demos"),
    ("Samples", "samples"),
    ("Export", "export"),
    ("FAQ", "faq"),
    ("Install", "install"),
    ("Home", "about"),
    ("Developer-Guide", "developer"),
]


def plain(text: str) -> str:
    """Markdown inline markup -> plain ASCII the 8x8 font can draw."""
    text = re.sub(r"<[^>]+>", "", text)                      # html (img, br)
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", text)          # images
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)      # links -> text
    text = text.replace("**", "").replace("`", "")
    text = re.sub(r"(?<![A-Za-z0-9])_([^_]+)_(?![A-Za-z0-9])", r"\1", text)
    replacements = {
        "—": "-", "–": "-", "→": "->", "←": "<-",
        "…": "...", "×": "x", "‘": "'", "’": "'",
        "“": '"', "”": '"', "•": "-", " ": " ",
    }
    for k, v in replacements.items():
        text = text.replace(k, v)
    return text.encode("ascii", "ignore").decode("ascii").strip()


def wrap(text: str, indent: str = "") -> list[str]:
    if not text:
        return [""]
    return textwrap.wrap(text, WIDTH, initial_indent=indent,
                         subsequent_indent=" " * len(indent) if indent else "",
                         break_long_words=True, break_on_hyphens=False) or [""]


def table_lines(rows: list[list[str]]) -> list[str]:
    """A markdown table as 'key: value' lines that fit the narrow screen."""
    out: list[str] = []
    header = rows[0]
    for row in rows[1:]:
        cells = [plain(c) for c in row]
        if not any(cells):
            continue
        key, rest = cells[0], [c for c in cells[1:] if c]
        if len(header) > 2 and len(rest) > 1:
            # Several columns: label the extra ones with their header
            parts = [rest[0]] + [f"{plain(h)}: {c}" for h, c in zip(header[2:], rest[1:]) if c]
        else:
            parts = rest
        out.append("*" + key[:WIDTH])
        for part in parts:
            out.extend(" " + line for line in wrap(part, "  "))
    return out


def convert(md: str) -> list[str]:
    lines = md.splitlines()
    out: list[str] = []
    i = 0
    in_code = False
    while i < len(lines):
        raw = lines[i].rstrip()
        i += 1
        if raw.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            text = raw.encode("ascii", "ignore").decode("ascii")
            for k in range(0, max(1, len(text)), WIDTH):
                out.append("|" + text[k:k + WIDTH])
            continue
        if raw.startswith("# "):
            continue  # page title comes from the topic
        if raw.startswith("## ") or raw.startswith("### "):
            title = plain(raw.lstrip("#").strip())
            if out and out[-1] != " ":
                out.append(" ")
            out.append("=" + title[:WIDTH])
            continue
        if raw.startswith("|"):
            rows = []
            j = i - 1
            while j < len(lines) and lines[j].strip().startswith("|"):
                cells = [c.strip() for c in lines[j].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-{2,}:?", c) for c in cells if c):
                    rows.append(cells)
                j += 1
            i = j
            if rows:
                out.extend(table_lines(rows))
            continue
        text = plain(raw)
        if not text:
            if out and out[-1] != " ":
                out.append(" ")
            continue
        if raw.lstrip().startswith(">"):
            out.extend("*" + line for line in wrap(plain(raw.lstrip()[1:])))
            continue
        bullet = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", raw)
        if bullet:
            mark = "-" if bullet.group(2) in "-*" else bullet.group(2)
            out.extend(" " + line for line in wrap(plain(bullet.group(3)), mark + " "))
            continue
        out.extend(" " + line for line in wrap(text))
    while out and out[-1] == " ":
        out.pop()
    return out


def title_of(md: str, fallback: str) -> str:
    for line in md.splitlines():
        if line.startswith("# "):
            return plain(line[2:])[:WIDTH]
    return fallback


def build() -> str:
    parts = ["# LGPT RG Nano built-in guide, generated by tools/build_ingame_guide.py"]
    for name, topic in PAGES:
        md = (WIKI / f"{name}.md").read_text(encoding="utf-8")
        fallback = "About this app" if name == "Home" else name.replace('-', ' ')
        parts.append(f"@{topic}|{title_of(md, fallback)}")
        parts.extend(convert(md))
    return "\n".join(parts) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if guide.txt is out of date")
    args = ap.parse_args()
    text = build()
    too_wide = [l for l in text.splitlines() if not l.startswith(("#", "@")) and len(l) > WIDTH + 1]
    if too_wide:
        print("lines wider than the screen:", *too_wide[:5], sep="\n  ")
        return 1
    if args.check:
        current = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if current != text:
            print(f"{OUT} is out of date: run python tools/build_ingame_guide.py")
            return 1
        return 0
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="ascii", newline="\n")
    pages = sum(1 for l in text.splitlines() if l.startswith("@"))
    print(f"wrote {OUT.relative_to(ROOT)}: {pages} pages, {len(text.splitlines())} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
