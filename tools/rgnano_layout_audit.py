#!/usr/bin/env python3
"""Programmatic RG Nano screenshot layout audit.

This is intentionally conservative. It does not try to understand whether a
screen is beautiful; it catches the tiny-screen failures that are easy to miss
by eye: wrong dimensions, clipped bright pixels on the edge, text/control rows
mashed together, and page-specific visual panels colliding with editable fields.
"""

from __future__ import annotations

import argparse
import glob
import os
import sys
from collections import deque
from dataclasses import dataclass
from typing import Iterable, List, Optional, Sequence, Tuple

from PIL import Image


Color = Tuple[int, int, int]
BBox = Tuple[int, int, int, int]


@dataclass
class Issue:
    path: str
    message: str


@dataclass
class Component:
    bbox: BBox
    pixels: int

    @property
    def width(self) -> int:
        return self.bbox[2] - self.bbox[0] + 1

    @property
    def height(self) -> int:
        return self.bbox[3] - self.bbox[1] + 1


def dist(a: Color, b: Color) -> int:
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])


def dominant_color(pixels: Sequence[Color]) -> Color:
    counts = {}
    for p in pixels:
        counts[p] = counts.get(p, 0) + 1
    return max(counts.items(), key=lambda item: item[1])[0]


def load_rgb(path: str) -> Image.Image:
    return Image.open(path).convert("RGB")


def foreground_mask(img: Image.Image, threshold: int = 34) -> List[List[bool]]:
    width, height = img.size
    get_pixels = getattr(img, "get_flattened_data", img.getdata)
    pixels = list(get_pixels())
    bg = dominant_color(pixels)
    mask: List[List[bool]] = []
    for y in range(height):
        row = []
        for x in range(width):
            row.append(dist(pixels[y * width + x], bg) >= threshold)
        mask.append(row)
    return mask


def components(mask: List[List[bool]], min_pixels: int = 20) -> List[Component]:
    height = len(mask)
    width = len(mask[0]) if height else 0
    seen = [[False] * width for _ in range(height)]
    found: List[Component] = []
    for y in range(height):
        for x in range(width):
            if seen[y][x] or not mask[y][x]:
                continue
            q = deque([(x, y)])
            seen[y][x] = True
            count = 0
            min_x = max_x = x
            min_y = max_y = y
            while q:
                cx, cy = q.popleft()
                count += 1
                min_x = min(min_x, cx)
                max_x = max(max_x, cx)
                min_y = min(min_y, cy)
                max_y = max(max_y, cy)
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    if seen[ny][nx] or not mask[ny][nx]:
                        continue
                    seen[ny][nx] = True
                    q.append((nx, ny))
            if count >= min_pixels:
                found.append(Component((min_x, min_y, max_x, max_y), count))
    return found


def row_ink(mask: List[List[bool]], row: int) -> int:
    y0 = row * 8
    y1 = min(y0 + 8, len(mask))
    if y0 >= y1:
        return 0
    return sum(1 for y in range(y0, y1) for x in range(len(mask[y])) if mask[y][x])


def row_active_cells(mask: List[List[bool]], row: int) -> int:
    y0 = row * 8
    y1 = min(y0 + 8, len(mask))
    width = len(mask[0])
    cells = 0
    for cell_x in range(width // 8):
        x0 = cell_x * 8
        x1 = x0 + 8
        ink = sum(1 for y in range(y0, y1) for x in range(x0, x1) if mask[y][x])
        if ink >= 4:
            cells += 1
    return cells


def rect_ink(mask: List[List[bool]], rect: BBox) -> int:
    x0, y0, x1, y1 = rect
    x0 = max(0, x0)
    y0 = max(0, y0)
    x1 = min(len(mask[0]) - 1, x1)
    y1 = min(len(mask) - 1, y1)
    return sum(1 for y in range(y0, y1 + 1) for x in range(x0, x1 + 1) if mask[y][x])


def max_run(values: Iterable[bool]) -> int:
    best = 0
    current = 0
    for value in values:
        if value:
            current += 1
            best = max(best, current)
        else:
            current = 0
    return best


def edge_run(mask: List[List[bool]], edge: str) -> int:
    width = len(mask[0])
    height = len(mask)
    if edge == "top":
        return max_run(mask[0])
    if edge == "bottom":
        return max_run(mask[height - 1])
    if edge == "left":
        return max_run(mask[y][0] for y in range(height))
    if edge == "right":
        return max_run(mask[y][width - 1] for y in range(height))
    return 0


def is_visual_workstation_screen(name: str) -> bool:
    return (
        "sample-lab-" in name
        or "sample-full-02-" in name
        or "sample-full-03-" in name
        or "sample-full-04-" in name
        or "sample-full-05-" in name
        or "sample-full-06-" in name
        or "sample-full-07-" in name
        or "sample-full-08-" in name
        or "sample-full-09-" in name
        or "sample-full-10-" in name
        or "sample-full-11-" in name
        or "sample-full-12-" in name
        or "sample-full-13-" in name
        or "sample-full-14-" in name
        or "audit-02-mixer" in name
        or "audit-06-phrase" in name
        or "audit-07-phrase-note" in name
    )


def is_framed_modal(name: str) -> bool:
    return (
        "audit-00-boot-project" in name
        or "audit-00-new-project" in name
        or "audit-12-sample-import" in name
        or "sample-full-01-import-browser" in name
        or name.startswith("sample-full-15-helper")
        or name.startswith("sample-full-16-helper")
    )


def dense_row_allowed(name: str, row: int) -> bool:
    if ("instrument-lab-" in name or "sample-lab-" in name or "sample-full-" in name) and 4 <= row <= 11:
        return True
    if is_framed_modal(name):
        if name.startswith("sample-full-15-helper") or name.startswith("sample-full-16-helper"):
            return row in (2, 25)
        if "sample-full-01-import-browser" in name:
            return row in (4, 25)
        if "audit-12-sample-import" in name:
            return row in (4, 25)
        return row in (2, 28)
    if "audit-02-mixer" in name:
        return 5 <= row <= 23
    return False


def audit_pixel_artifacts(path: str, mask: List[List[bool]]) -> List[Issue]:
    issues: List[Issue] = []
    name = os.path.basename(path).lower()
    if is_visual_workstation_screen(name) or "audit-14-power-menu" in name:
        return issues

    for comp in components(mask, min_pixels=90):
        # Inverted tracker text produces legitimate 8px-tall solid selection
        # rows. Stale waveform/meter artifacts are taller pixel-drawn blocks.
        if comp.width < 20 or comp.height < 10:
            continue
        area = comp.width * comp.height
        density = comp.pixels / float(area)
        if density >= 0.58:
            issues.append(Issue(
                path,
                "large solid pixel block outside a visual screen "
                f"(bbox={comp.bbox}, density={density:.2f}); likely stale bar/wave artifact",
            ))
    return issues


def audit_generic(path: str, img: Image.Image, mask: List[List[bool]]) -> List[Issue]:
    issues: List[Issue] = []
    name = os.path.basename(path).lower()
    if img.size != (240, 240):
        issues.append(Issue(path, f"expected 240x240 app screenshot, got {img.size[0]}x{img.size[1]}"))

    # Long continuous edge runs usually mean a visual panel is clipped. Modal
    # frames are intentional and get screen-specific checks below.
    if not is_framed_modal(name):
        edge_runs = {edge: edge_run(mask, edge) for edge in ("top", "bottom", "left", "right")}
        bad_edges = [f"{edge}={run}" for edge, run in edge_runs.items() if run >= 80]
        if bad_edges:
            issues.append(Issue(path, f"large continuous foreground on edge ({', '.join(bad_edges)}); likely clipped"))

    for row in range(30):
        if dense_row_allowed(name, row):
            continue
        active = row_active_cells(mask, row)
        ink = row_ink(mask, row)
        if active >= 28 and ink > 900:
            issues.append(Issue(path, f"row {row} is nearly full and dense ({active} cells, {ink}px); likely unreadable"))

    return issues


def require_row(issues: List[Issue], path: str, mask: List[List[bool]], row: int,
                label: str, minimum: int = 32) -> None:
    ink = row_ink(mask, row)
    if ink < minimum:
        issues.append(Issue(path, f"missing/too faint {label} on row {row} ({ink}px)"))


def require_rect(issues: List[Issue], path: str, mask: List[List[bool]], rect: BBox,
                 label: str, minimum: int) -> None:
    ink = rect_ink(mask, rect)
    if ink < minimum:
        issues.append(Issue(path, f"missing/too faint {label} in rect {rect} ({ink}px)"))


def audit_producer_screen(path: str, mask: List[List[bool]]) -> List[Issue]:
    issues: List[Issue] = []
    name = os.path.basename(path).lower()
    if not name.startswith("audit-"):
        return issues

    if "audit-00-boot-project" in name:
        require_row(issues, path, mask, 5, "project list", 40)
        require_row(issues, path, mask, 26, "Load/New/Exit action row", 80)
    elif "audit-00-new-project" in name:
        require_row(issues, path, mask, 7, "new project dialog content", 40)
        require_row(issues, path, mask, 26, "dialog actions", 40)
    elif "audit-01-song" in name or "audit-03-song-chain" in name:
        require_row(issues, path, mask, 0, "song title/status", 40)
        require_rect(issues, path, mask, (0, 32, 220, 160), "song chain grid", 500)
        require_rect(issues, path, mask, (16, 176, 216, 224), "song channel monitor", 250)
    elif "audit-02-mixer" in name:
        require_row(issues, path, mask, 0, "mixer title/status", 40)
        require_row(issues, path, mask, 4, "mixer channel labels", 80)
        require_rect(issues, path, mask, (20, 48, 220, 128), "mixer channel meters", 250)
    elif "audit-04-chain" in name or "audit-05-chain-phrase" in name:
        require_row(issues, path, mask, 0, "chain title/status", 40)
        require_rect(issues, path, mask, (0, 32, 180, 160), "chain rows", 350)
    elif "audit-06-phrase" in name or "audit-07-phrase-note" in name:
        require_row(issues, path, mask, 0, "phrase title/status", 40)
        require_rect(issues, path, mask, (0, 32, 236, 164), "phrase note/effect grid", 900)
        require_rect(issues, path, mask, (16, 184, 216, 224), "phrase mini waveform/monitor", 160)
    elif "audit-08-table" in name:
        require_row(issues, path, mask, 0, "table title/status", 40)
        require_rect(issues, path, mask, (0, 32, 236, 164), "table effect grid", 700)
    elif "audit-09-groove" in name:
        require_row(issues, path, mask, 0, "groove title/status", 40)
        require_rect(issues, path, mask, (0, 32, 180, 164), "groove rows", 300)
    elif "audit-10-instrument" in name:
        require_row(issues, path, mask, 0, "instrument title/status", 40)
        require_row(issues, path, mask, 2, "instrument lab title", 40)
        require_row(issues, path, mask, 17, "instrument editable fields", 40)
    elif "audit-11-table-instrument" in name:
        require_row(issues, path, mask, 0, "instrument table title/status", 40)
        require_rect(issues, path, mask, (0, 32, 236, 164), "instrument table rows", 700)
    elif "audit-12-sample-import" in name:
        require_row(issues, path, mask, 7, "sample import file list", 80)
        require_row(issues, path, mask, 20, "sample import selected item info", 80)
        require_row(issues, path, mask, 21, "sample import selected item name", 40)
        require_row(issues, path, mask, 23, "sample import actions", 120)
    elif "audit-13-project" in name:
        require_row(issues, path, mask, 0, "project title/status", 80)
        require_row(issues, path, mask, 5, "tempo/master settings", 80)
        require_rect(issues, path, mask, (0, 40, 230, 220), "project settings list", 700)
    elif "audit-14-power-menu" in name:
        require_rect(issues, path, mask, (28, 78, 212, 148), "power menu dialog", 600)

    return issues


def largest_visual_component(mask: List[List[bool]], min_width: int = 80,
                             max_top: int = 120) -> Optional[Component]:
    comps = [
        c for c in components(mask, min_pixels=80)
        if c.width >= min_width and c.height >= 10 and c.bbox[1] <= max_top
    ]
    if not comps:
        return None
    return max(comps, key=lambda c: c.pixels)


def audit_sample_lab(path: str, mask: List[List[bool]]) -> List[Issue]:
    issues: List[Issue] = []
    name = os.path.basename(path).lower()
    if "sample-full-15-helper" in name or "sample-full-16-helper" in name:
        return issues
    if "instrument-lab-" not in name and "sample-lab-" not in name and "sample-full-" not in name:
        return issues

    # Rows 0-3 are title/status. Visual controls should live below that.
    comp = largest_visual_component(mask, min_width=8 if "motion" in name else 80)
    if comp is None:
        issues.append(Issue(path, "no substantial visual/control component detected"))
        return issues

    x0, y0, x1, y1 = comp.bbox
    if "source" in name or "loop" in name or "marker-edit" in name:
        if y0 < 32:
            issues.append(Issue(path, f"waveform starts too high at y={y0}; collides with title/status area"))
        if y1 > 96:
            issues.append(Issue(path, f"waveform ends too low at y={y1}; collides with legend/field area"))

        # Source/Loop now keep the marker legend in the page-title band and
        # reserve rows 12-16 as breathing room before the parameter list.
        legend_ink = row_ink(mask, 2)
        sample_or_value_ink = row_ink(mask, 17)
        first_field_ink = row_ink(mask, 17)
        spacer_ink = row_ink(mask, 16)
        if legend_ink < 40:
            issues.append(Issue(path, "missing/too faint marker legend row"))
        if "marker-edit" not in name and sample_or_value_ink < 40:
            issues.append(Issue(path, "missing sample name, loop value, or marker summary row"))
        if first_field_ink < 40:
            issues.append(Issue(path, "editable field rows did not start where expected"))
        if spacer_ink > 120:
            issues.append(Issue(path, f"spacer row before editable fields is crowded ({spacer_ink}px)"))

    if "shape" in name or "filter" in name:
        # Three horizontal controls should fit above row 13 and leave the field
        # list below row 17 readable.
        if y0 < 38:
            issues.append(Issue(path, f"control panel starts too high at y={y0}"))
        if y1 > 106:
            issues.append(Issue(path, f"control panel ends too low at y={y1}; field rows may be mashed"))
        if row_ink(mask, 14) < 40:
            issues.append(Issue(path, "missing parameter summary row above editable fields"))
        if row_ink(mask, 16) > 120:
            issues.append(Issue(path, "crowded spacer row before editable fields"))

    if "motion" in name:
        if y1 > 110:
            issues.append(Issue(path, f"motion visualization ends too low at y={y1}; field rows may be mashed"))
        if row_ink(mask, 13) < 40:
            issues.append(Issue(path, "missing table automation summary row"))

    return issues


def audit_guide(path: str, img: Image.Image) -> List[Issue]:
    """The built-in guide (guide-*.bmp) draws its own pixel layout: text on
    10 px lines, examples on tinted panels, a scroll bar on the right. Checks
    that text stays inside its margins, clear of the scroll bar, and that no
    two lines touch (lines that overlap or run together)."""
    issues: List[Issue] = []
    if img.size != (240, 240):
        return [Issue(path, f"expected 240x240 app screenshot, got {img.size[0]}x{img.size[1]}")]
    width, height = img.size
    get_pixels = getattr(img, "get_flattened_data", img.getdata)
    pixels = list(get_pixels())
    bg = dominant_color(pixels)
    # Example panels are a dark tint of the background: a second background
    tints = [p for p in pixels if 0 < dist(p, bg) < 90]
    panel = dominant_color(tints) if len(tints) > 400 else bg

    def ink(x: int, y: int) -> bool:
        p = pixels[y * width + x]
        return dist(p, bg) >= 34 and dist(p, panel) >= 34

    rules = {14, 218}  # title and footer rules span the screen on purpose
    for y in range(height):
        if y in rules:
            continue
        for x in (0, 1, 238, 239):
            if ink(x, y):
                issues.append(Issue(path, f"ink at the screen edge ({x},{y}); text or panel clipped"))
                break
    # Text ends by x=232, the scroll bar starts at 235: nothing in between
    for y in range(18, 217):
        if ink(233, y) or ink(234, y):
            issues.append(Issue(path, f"text touches the scroll bar at y={y}"))
            break
    # Lines of 8 px letters have air between them; a taller run of inked
    # rows means two lines touch. Selection bars (mostly filled rows) are
    # their own background and end a run; a popup's side borders (columns
    # inked most of the way down) are frame, not text.
    body = range(18, 217)
    frame = {x for x in range(3, 233) if sum(1 for y in body if ink(x, y)) > 0.4 * len(body)}
    run = 0
    for y in body:
        count = sum(1 for x in range(3, 233) if x not in frame and ink(x, y))
        if count > 0.6 * 230:
            run = 0
            continue
        run = run + 1 if count else 0
        if run > 9:
            issues.append(Issue(path, f"lines touch around y={y}; text overlaps or has no line gap"))
            run = 0
    return issues


def audit_paths(paths: Iterable[str]) -> List[Issue]:
    issues: List[Issue] = []
    for path in paths:
        img = load_rgb(path)
        if os.path.basename(path).lower().startswith("guide-"):
            issues.extend(audit_guide(path, img))
            continue
        mask = foreground_mask(img)
        issues.extend(audit_generic(path, img, mask))
        issues.extend(audit_pixel_artifacts(path, mask))
        issues.extend(audit_producer_screen(path, mask))
        issues.extend(audit_sample_lab(path, mask))
    return issues


def expand_inputs(inputs: Sequence[str]) -> List[str]:
    paths: List[str] = []
    for item in inputs:
        matches = glob.glob(item)
        if matches:
            paths.extend(matches)
        elif os.path.exists(item):
            paths.append(item)
        else:
            raise FileNotFoundError(item)
    return sorted(set(paths))


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Audit RG Nano app screenshots for cramped/unreadable layout.")
    parser.add_argument("paths", nargs="+", help="BMP/PNG paths or globs to audit")
    args = parser.parse_args(argv)

    try:
        paths = expand_inputs(args.paths)
    except FileNotFoundError as exc:
        print(f"layout audit: missing input {exc}", file=sys.stderr)
        return 2

    issues = audit_paths(paths)
    if issues:
        print("RG Nano layout audit failed:")
        for issue in issues:
            print(f"- {issue.path}: {issue.message}")
        return 1

    print(f"RG Nano layout audit passed ({len(paths)} screenshot(s)).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
