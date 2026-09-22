#!/usr/bin/env python3
"""Build the user guide as a static HTML site from docs/rgnano-wiki/*.md.

The Markdown pages stay the single source of truth (they also work as a
GitHub wiki). This renders them into a small styled site with a sidebar,
suitable for GitHub Pages:

    python tools/build_guide_site.py            # -> build/guide-site
    python tools/build_guide_site.py --out DIR
"""

from __future__ import annotations

import argparse
import html
import re
import shutil
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "rgnano-wiki"
DEFAULT_OUT = ROOT / "build" / "guide-site"

REPO_URL = "https://github.com/Inkmortal/LittleGPTracker-RG-Nano-Audio-In-Sampling"

CSS = r"""
:root {
  --bg: #120614;
  --panel: #1d0a1f;
  --panel-2: #26102a;
  --line: #3d1a42;
  --text: #efe4f5;
  --muted: #b9a3c2;
  --accent: #e040e0;
  --accent-2: #ff8af5;
  --code: #2a1030;
  --radius: 10px;
}
@media (prefers-color-scheme: light) {
  :root:not([data-theme="dark"]) {
    --bg: #faf6fb; --panel: #ffffff; --panel-2: #f3e9f5; --line: #e2cfe6;
    --text: #22122a; --muted: #6b5673; --accent: #b3179f; --accent-2: #8c0f7c; --code: #f3e9f5;
  }
}
:root[data-theme="light"] {
  --bg: #faf6fb; --panel: #ffffff; --panel-2: #f3e9f5; --line: #e2cfe6;
  --text: #22122a; --muted: #6b5673; --accent: #b3179f; --accent-2: #8c0f7c; --code: #f3e9f5;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body {
  margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.65 Inter, "Segoe UI", system-ui, -apple-system, sans-serif;
}
a { color: var(--accent-2); text-decoration: none; }
a:hover { text-decoration: underline; }
.layout { display: grid; grid-template-columns: 260px minmax(0, 1fr); min-height: 100vh; }
nav.side {
  position: sticky; top: 0; height: 100vh; overflow-y: auto;
  background: var(--panel); border-right: 1px solid var(--line); padding: 24px 20px;
}
nav.side .brand { display: flex; align-items: center; gap: 10px; margin-bottom: 20px; }
nav.side .brand img { width: 44px; height: 44px; image-rendering: pixelated; border-radius: 8px; }
nav.side .brand span { font-weight: 800; letter-spacing: .02em; line-height: 1.2; }
nav.side .brand small { display: block; color: var(--muted); font-weight: 500; }
nav.side h4 {
  margin: 18px 0 6px; font-size: 12px; text-transform: uppercase; letter-spacing: .12em; color: var(--muted);
}
nav.side ul { list-style: none; margin: 0; padding: 0; }
nav.side li a {
  display: block; padding: 5px 10px; border-radius: 6px; color: var(--text);
}
nav.side li a:hover { background: var(--panel-2); text-decoration: none; }
nav.side li a.active { background: var(--accent); color: #fff; font-weight: 600; }
main { padding: 40px min(6vw, 64px) 80px; max-width: 980px; width: 100%; }
main h1 { font-size: 2.1rem; line-height: 1.2; margin: 0 0 18px; letter-spacing: -.01em; }
main h1:first-child { margin-top: 0; }
main h2 {
  font-size: 1.35rem; margin: 42px 0 12px; padding-bottom: 6px; border-bottom: 1px solid var(--line);
}
main h3 { font-size: 1.1rem; margin: 30px 0 8px; color: var(--accent-2); }
main p, main li { color: var(--text); }
main img { max-width: 100%; image-rendering: pixelated; border-radius: 8px; border: 1px solid var(--line); }
main img[align="right"] { float: right; margin: 0 0 16px 24px; max-width: 42%; }
br[clear] { clear: both; display: block; content: ""; }
p[align="center"] { text-align: center; }
p[align="center"] img { margin: 4px; }
h1[align="center"] { text-align: center; }
table {
  border-collapse: collapse; margin: 16px 0 22px; font-size: .95rem;
  background: var(--panel); border-radius: var(--radius); overflow: hidden; display: table;
  /* auto width lets a table sit beside a floated screenshot instead of below it */
  width: auto; max-width: 100%;
}
th, td { padding: 9px 12px; text-align: left; vertical-align: top; border-bottom: 1px solid var(--line); }
th { background: var(--panel-2); font-weight: 700; }
tr:last-child td { border-bottom: 0; }
td img { max-width: 220px; }
code {
  font-family: "JetBrains Mono", Consolas, "Cascadia Mono", monospace; font-size: .88em;
  background: var(--code); padding: 2px 6px; border-radius: 5px;
}
pre {
  background: var(--code); padding: 14px 16px; border-radius: var(--radius);
  overflow-x: auto; border: 1px solid var(--line); line-height: 1.5;
}
pre code { background: none; padding: 0; font-size: .86rem; }
blockquote {
  margin: 18px 0; padding: 12px 16px; border-left: 4px solid var(--accent);
  background: var(--panel); border-radius: 0 var(--radius) var(--radius) 0;
}
blockquote p { margin: 0; }
hr { border: 0; border-top: 1px solid var(--line); margin: 28px 0; }
footer { margin-top: 56px; padding-top: 16px; border-top: 1px solid var(--line); color: var(--muted); font-size: .9rem; }
.pager { display: flex; justify-content: space-between; gap: 12px; margin-top: 48px; }
.pager a {
  flex: 1; padding: 14px 16px; border: 1px solid var(--line); border-radius: var(--radius);
  background: var(--panel); color: var(--text);
}
.pager a:hover { border-color: var(--accent); text-decoration: none; }
.pager small { display: block; color: var(--muted); }
.pager .next { text-align: right; }
.menu-toggle { display: none; }
.theme-toggle {
  margin-top: 22px; width: 100%; padding: 8px; border-radius: 6px; cursor: pointer;
  background: var(--panel-2); color: var(--text); border: 1px solid var(--line); font: inherit;
}
@media (max-width: 860px) {
  .layout { grid-template-columns: 1fr; }
  nav.side { position: static; height: auto; border-right: 0; border-bottom: 1px solid var(--line); }
  nav.side .links { display: none; }
  nav.side.open .links { display: block; }
  .menu-toggle {
    display: block; margin-left: auto; padding: 6px 12px; border-radius: 6px; cursor: pointer;
    background: var(--panel-2); color: var(--text); border: 1px solid var(--line); font: inherit;
  }
  main { padding: 24px 16px 60px; }
  main img[align="right"] { float: none; display: block; margin: 12px 0; max-width: 100%; }
  table { display: block; overflow-x: auto; }
}
"""

SCRIPT = r"""
(function () {
  var root = document.documentElement;
  try { var t = localStorage.getItem('guide-theme'); if (t) root.setAttribute('data-theme', t); } catch (e) {}
  document.addEventListener('DOMContentLoaded', function () {
    var btn = document.querySelector('.theme-toggle');
    if (btn) btn.addEventListener('click', function () {
      var dark = root.getAttribute('data-theme') === 'dark' ||
        (!root.getAttribute('data-theme') && window.matchMedia('(prefers-color-scheme: dark)').matches);
      var next = dark ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem('guide-theme', next); } catch (e) {}
    });
    var menu = document.querySelector('.menu-toggle');
    if (menu) menu.addEventListener('click', function () {
      document.querySelector('nav.side').classList.toggle('open');
    });
  });
})();
"""


def page_file(name: str) -> str:
    return "index.html" if name == "Home" else f"{name}.html"


def rewrite_links(body: str, pages: set[str]) -> str:
    """Turn wiki-style links (href="Install", "Screens#mixer") into .html files."""
    def fix(match: re.Match) -> str:
        target = match.group(1)
        if re.match(r"^[a-z]+:|^#|^/", target) or "." in target.split("#")[0]:
            return match.group(0)
        name, _, anchor = target.partition("#")
        if name not in pages:
            return match.group(0)
        return f'href="{page_file(name)}' + (f"#{anchor}" if anchor else "") + '"'
    return re.sub(r'href="([^"]+)"', fix, body)


def parse_sidebar(text: str) -> list[tuple[str, list[tuple[str, str]]]]:
    sections: list[tuple[str, list[tuple[str, str]]]] = []
    for line in text.splitlines():
        head = re.match(r"^\*\*(.+)\*\*$", line.strip())
        link = re.match(r"^- \[(.+?)\]\((.+?)\)", line.strip())
        if head:
            sections.append((head.group(1), []))
        elif link and sections:
            sections[-1][1].append((link.group(1), link.group(2)))
    return sections


def render(md_text: str) -> str:
    return markdown.markdown(md_text, extensions=["tables", "fenced_code", "toc", "attr_list", "md_in_html", "sane_lists"],
                             extension_configs={"toc": {"permalink": False}}, output_format="html5")


def title_of(md_text: str, fallback: str) -> str:
    for line in md_text.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
        m = re.search(r"<h1[^>]*>(.*?)</h1>", line)
        if m:
            return re.sub("<[^>]+>", "", m.group(1)).strip()
    return fallback.replace("-", " ")


def build(out: Path) -> int:
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copytree(SOURCE / "images", out / "images")
    (out / "style.css").write_text(CSS.strip() + "\n", encoding="utf-8")
    (out / "site.js").write_text(SCRIPT.strip() + "\n", encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")

    sources = {p.stem: p.read_text(encoding="utf-8") for p in SOURCE.glob("*.md") if not p.stem.startswith("_")}
    pages = set(sources)
    sidebar = parse_sidebar((SOURCE / "_Sidebar.md").read_text(encoding="utf-8"))
    footer = rewrite_links(render((SOURCE / "_Footer.md").read_text(encoding="utf-8")), pages)
    order = ["Home"] + [target for _, links in sidebar for _, target in links if target != "Home"]

    for name, text in sources.items():
        title = title_of(text, name)
        body = rewrite_links(render(text), pages)
        nav_html = []
        for section, links in sidebar:
            nav_html.append(f"<h4>{html.escape(section)}</h4><ul>")
            for label, target in links:
                cls = ' class="active"' if target == name else ""
                nav_html.append(f'<li><a href="{page_file(target)}"{cls}>{html.escape(label)}</a></li>')
            nav_html.append("</ul>")
        pager = ""
        if name in order:
            i = order.index(name)
            prev_link = (f'<a class="prev" href="{page_file(order[i - 1])}"><small>Previous</small>'
                         f'{html.escape(order[i - 1].replace("-", " "))}</a>') if i > 0 else "<span></span>"
            next_link = (f'<a class="next" href="{page_file(order[i + 1])}"><small>Next</small>'
                         f'{html.escape(order[i + 1].replace("-", " "))}</a>') if i + 1 < len(order) else "<span></span>"
            pager = f'<div class="pager">{prev_link}{next_link}</div>'
        page_title = "LGPT for RG Nano" if name == "Home" else f"{title} · LGPT for RG Nano"
        doc = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(page_title)}</title>
<meta name="description" content="LGPT for RG Nano: a pocket tracker with built-in synths. {html.escape(title)}.">
<link rel="icon" href="images/synth-1-sound.png">
<link rel="stylesheet" href="style.css">
<script src="site.js"></script>
</head>
<body>
<div class="layout">
<nav class="side">
  <div class="brand"><img src="images/demo-song-playing.png" alt=""><span>LGPT for RG Nano<small>pocket tracker guide</small></span>
    <button class="menu-toggle" aria-label="Menu">Menu</button></div>
  <div class="links">
  {''.join(nav_html)}
  <h4>Project</h4><ul><li><a href="{REPO_URL}">Source code</a></li></ul>
  <button class="theme-toggle" type="button">Light / dark</button>
  </div>
</nav>
<main>
{body}
{pager}
<footer>{footer}</footer>
</main>
</div>
</body>
</html>
"""
        (out / page_file(name)).write_text(doc, encoding="utf-8")
    print(f"built {len(sources)} pages into {out}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    return build(args.out)


if __name__ == "__main__":
    raise SystemExit(main())
