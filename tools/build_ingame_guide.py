#!/usr/bin/env python3
"""Turn the user guide (docs/rgnano-wiki/*.md) into the app's built-in guide.

Writes projects/resources/guide/guide.txt, which ships next to the binary
(OPK and simulator) and is read by the in-app Guide (Help button on the
start screen, A in the RB+Select helper). Run it whenever the wiki changes;
tools/install-rgnano.ps1 and the CI build run it too.

Format (one entry per line, first character is the kind):
    @<id>|<title>    a page (topic)
    =<title>         a section (##): a Left/Right stop, drawn as a heading
    +<title>         a subsection (###): also a stop, a smaller heading
    ' '<text>        body text, already wrapped to WIDTH columns
    ' ' alone        a paragraph gap (drawn as half a line)
    '|'<text>        code / diagram text, drawn in a tinted box
    '%'<text>        a tracker example (```phrase, ```chain, ```song,
                     ```table fences), drawn like the tracker screen
    '*'<text>        a table key (the term the lines under it explain)
    '>'<text>        a tip (a > quote), drawn with a bar on its left
    '~'<col>,<len>,<page>,<section>
                     the text line above has a link: <len> characters from
                     column <col> go to page id <page>, at heading
                     <section> (empty: the top of the page)

Readers of the old format see nothing new they must handle except the
'+', '%', '>' and '~' kinds, which they can draw as plain text / ignore.

Wrapping keeps key combos ("RB + Right"), note names ("C 3") and hex
ranges together, so a line never ends on "RB +".

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

WIDTH = 28  # characters across the guide's text area (240 px screen, 8 px font)

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
PAGE_IDS = {name: topic for name, topic in PAGES}

EXAMPLE_FENCES = {"phrase", "chain", "song", "table", "tracker"}

NBSP = "\x01"  # a space the wrapper must not break at

REPLACEMENTS = {
    "—": "-", "–": "-", "→": "->", "←": "<-",
    "…": "...", "×": "x", "‘": "'", "’": "'",
    "“": '"', "”": '"', "•": "-", " ": " ",
    "└": "`", "├": "|", "│": "|", "─": "-", "┬": "-", "┌": ",", "┐": ",",
    "┘": "'", "°": "o", "±": "+/-", "½": "1/2", "♯": "#", "♭": "b",
}


def ascii_text(text: str) -> str:
    for k, v in REPLACEMENTS.items():
        text = text.replace(k, v)
    return text.encode("ascii", "ignore").decode("ascii")


def slug(title: str) -> str:
    """GitHub's heading anchor: lower case, punctuation dropped, spaces -> -."""
    s = title.strip().lower()
    s = re.sub(r"[^\w\- ]", "", s)
    return s.replace(" ", "-")


def plain_with_links(text: str, page: str) -> tuple[str, list[tuple[str, str, str]]]:
    """Markdown inline markup -> plain ASCII, plus the links it held as
    (label, target page id, target heading slug)."""
    links: list[tuple[str, str, str]] = []
    text = re.sub(r"<[^>]+>", "", text)                      # html (img, br)
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", text)          # images

    def link(m: re.Match) -> str:
        label, target = m.group(1), m.group(2).strip()
        if not re.match(r"^[a-z]+:", target):                 # not http(s):
            name, _, anchor = target.partition("#")
            name = name.removesuffix(".md")
            topic = PAGE_IDS.get(name or page)
            if topic:
                links.append((label, topic, anchor))
        return label

    text = re.sub(r"\[([^\]]+)\]\(([^)]*)\)", link, text)
    text = text.replace("**", "").replace("`", "")
    text = re.sub(r"(?<![A-Za-z0-9])_([^_]+)_(?![A-Za-z0-9])", r"\1", text)
    text = ascii_text(text).strip()
    links = [(ascii_text(label.replace("**", "").replace("`", "")).strip(), topic, anchor)
             for label, topic, anchor in links]
    return text, links


def plain(text: str) -> str:
    return plain_with_links(text, "")[0]


def keep_together(text: str) -> str:
    """Spaces the wrapper must not break at: 'RB + Right', 'A + Start',
    note names 'C 3' and 'D#3', 'LB + Up/Down'."""
    text = re.sub(r"(\S) \+ (?=\S)", lambda m: m.group(1) + NBSP + "+" + NBSP, text)
    text = re.sub(r"\b([A-G]) (\d)\b", r"\1" + NBSP + r"\2", text)
    return text


def wrap(text: str, indent: str = "", width: int = WIDTH) -> list[str]:
    if not text:
        return [""]
    lines = textwrap.wrap(keep_together(text), width, initial_indent=indent,
                          subsequent_indent=" " * len(indent) if indent else "",
                          break_long_words=True, break_on_hyphens=False) or [""]
    return [line.replace(NBSP, " ") for line in lines]


class Output:
    def __init__(self, headings: dict[str, dict[str, str]]):
        self.lines: list[str] = []
        self.headings = headings  # topic -> slug -> title
        self.warnings: list[str] = []

    def gap(self) -> None:
        if self.lines and self.lines[-1] != " " and not self.lines[-1].startswith(("=", "+")):
            self.lines.append(" ")

    def text(self, kind: str, text: str, links: list[tuple[str, str, str]], indent: str = "") -> None:
        """Wrapped text lines, each followed by its links' '~' records."""
        wrapped = wrap(text, indent)
        flat = "\n".join(wrapped)
        pos = 0
        spans: dict[int, list[str]] = {}
        for label, topic, anchor in links:
            words = [re.escape(w) for w in label.split()]
            if not words:
                continue
            m = re.compile(r"\s+".join(words)).search(flat, pos)
            if not m:
                continue
            pos = m.end()
            section = self.headings.get(topic, {}).get(anchor, "") if anchor else ""
            if anchor and not section:
                self.warnings.append(f"link to missing heading {topic}#{anchor}")
            # Split a link that wraps onto the next line into one span per line
            start = m.start()
            while start < m.end():
                line_no = flat.count("\n", 0, start)
                line_start = flat.rfind("\n", 0, start) + 1
                line_end = flat.find("\n", start)
                if line_end < 0:
                    line_end = len(flat)
                end = min(m.end(), line_end)
                spans.setdefault(line_no, []).append(
                    f"~{start - line_start},{end - start},{topic},{section}")
                start = line_end + 1
        for n, line in enumerate(wrapped):
            self.lines.append(kind + line)
            self.lines.extend(spans.get(n, []))


def table_lines(out: Output, rows: list[list[str]], page: str) -> None:
    """A markdown table as 'key' + indented explanation: fits the narrow screen."""
    header = rows[0]
    for row in rows[1:]:
        cells = [plain_with_links(c, page) for c in row]
        if not any(c[0] for c in cells):
            continue
        key, rest = cells[0], [c for c in cells[1:] if c[0]]
        if len(header) > 2 and len(rest) > 1:
            # Several columns: label the extra ones with their header
            parts = [rest[0]] + [(f"{plain(h)}: {c[0]}", c[1])
                                 for h, c in zip(header[2:], rest[1:]) if c[0]]
        else:
            parts = rest
        head = key[0]
        # A short value joins its key: "VOLM  aabb" (two spaces: the index
        # takes the key before them)
        if parts and len(parts[0][0]) <= 10 and len(head) + 2 + len(parts[0][0]) <= WIDTH:
            head, parts = f"{head}  {parts[0][0]}", parts[1:]
        out.text("*", head, key[1])
        for text, links in parts:
            out.text(" ", text, links, "  ")


def code_lines(out: Output, block: list[str], fence: str, page: str) -> None:
    kind = "%" if fence in EXAMPLE_FENCES else "|"
    for raw in block:
        text = ascii_text(raw.rstrip())
        if len(text) <= WIDTH:
            out.lines.append(kind + text)
            continue
        # Too wide for the screen: wrap at spaces, continuing under the text
        out.warnings.append(f"code line wider than {WIDTH}: {text!r}")
        # (an indent as wide as the screen would never fit a word: cap it)
        lead = min(len(text) - len(text.lstrip()), WIDTH // 2)
        cont = " " * min(lead + 2, WIDTH // 2)
        for line in textwrap.wrap(text.strip(), WIDTH, initial_indent=" " * lead, subsequent_indent=cont,
                                  break_long_words=True, break_on_hyphens=False) or [""]:
            out.lines.append(kind + line)


def convert(md: str, page: str, headings: dict[str, dict[str, str]]) -> Output:
    lines = md.splitlines()
    out = Output(headings)
    i = 0
    while i < len(lines):
        raw = lines[i].rstrip()
        i += 1
        if raw.startswith("```"):
            fence = raw[3:].strip().lower()
            block = []
            while i < len(lines) and not lines[i].startswith("```"):
                block.append(lines[i])
                i += 1
            i += 1
            out.gap()
            code_lines(out, block, fence, page)
            out.gap()
            continue
        if raw.startswith("# "):
            continue  # page title comes from the topic
        if raw.startswith("## ") or raw.startswith("### "):
            title = plain(raw.lstrip("#").strip())
            while out.lines and out.lines[-1] == " ":
                out.lines.pop()
            out.lines.append(("=" if raw.startswith("## ") else "+") + title)
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
                out.gap()
                table_lines(out, rows, page)
                out.gap()
            continue
        text, links = plain_with_links(raw, page)
        if not text:
            out.gap()
            continue
        if raw.lstrip().startswith(">"):
            out.gap()
            text, links = plain_with_links(raw.lstrip()[1:], page)
            out.text(">", text, links)
            continue
        bullet = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", raw)
        if bullet:
            mark = "-" if bullet.group(2) in "-*" else bullet.group(2)
            nested = "  " if len(bullet.group(1)) >= 2 else ""
            text, links = plain_with_links(bullet.group(3), page)
            out.text(" ", text, links, nested + mark + " ")
            continue
        out.text(" ", text, links)
    while out.lines and out.lines[-1] == " ":
        out.lines.pop()
    return out


def title_of(md: str, fallback: str) -> str:
    for line in md.splitlines():
        if line.startswith("# "):
            return plain(line[2:])
    return fallback


def page_headings() -> dict[str, dict[str, str]]:
    """topic -> {GitHub anchor slug: heading as the guide shows it}."""
    result: dict[str, dict[str, str]] = {}
    for name, topic in PAGES:
        md = (WIKI / f"{name}.md").read_text(encoding="utf-8")
        found: dict[str, str] = {}
        in_code = False
        for line in md.splitlines():
            if line.startswith("```"):
                in_code = not in_code
            if not in_code and (line.startswith("## ") or line.startswith("### ")):
                raw = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", line.lstrip("#").strip())
                found.setdefault(slug(raw.replace("**", "").replace("`", "")), plain(raw))
        result[topic] = found
    return result


def build() -> tuple[str, list[str]]:
    headings = page_headings()
    parts = ["# LGPT RG Nano built-in guide, generated by tools/build_ingame_guide.py"]
    warnings: list[str] = []
    for name, topic in PAGES:
        md = (WIKI / f"{name}.md").read_text(encoding="utf-8")
        fallback = "About this app" if name == "Home" else name.replace('-', ' ')
        parts.append(f"@{topic}|{title_of(md, fallback)}")
        out = convert(md, name, headings)
        parts.extend(out.lines)
        warnings.extend(f"{name}: {w}" for w in out.warnings)
    return "\n".join(parts) + "\n", warnings


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if guide.txt is out of date")
    args = ap.parse_args()
    text, warnings = build()
    for w in warnings:
        print("warning:", w)
    too_wide = [l for l in text.splitlines()
                if l[:1] in (" ", "|", "%", "*", ">") and len(l) > WIDTH + 1]
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
