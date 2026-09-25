#!/usr/bin/env python3
"""Build dist/lgpt-rgnano-<commit>.zip in the layout docs/rgnano-wiki/Install.md
describes: the OPK, the demo songs, the sample packs and the guide.

Run after tools/install-rgnano.ps1 (or a device build) so the OPK is fresh:
    python tools/make_release_zip.py
"""

from __future__ import annotations

import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECTS = ROOT / "projects"
WIKI = ROOT / "docs" / "rgnano-wiki"


def main() -> int:
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"],
                            capture_output=True, text=True).stdout.strip()
    opk = PROJECTS / "lgpt-rgnano.opk"
    if not opk.exists():
        raise SystemExit("projects/lgpt-rgnano.opk missing: build it with tools/install-rgnano.ps1 first")
    out = ROOT / "dist" / f"lgpt-rgnano-{commit}.zip"
    out.parent.mkdir(exist_ok=True)
    base = "lgpt-rgnano"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(opk, f"{base}/lgpt-rgnano.opk")
        for demo in sorted((PROJECTS / "resources" / "demos").iterdir()):
            for f in demo.rglob("*"):
                if f.is_file() and f.name != ".installed.hash":
                    z.write(f, f"{base}/Tracks/{demo.name}/{f.relative_to(demo).as_posix()}")
        for pack in sorted((PROJECTS / "resources" / "samples").iterdir()):
            for f in pack.iterdir():
                if f.suffix in (".wav", ".md"):
                    z.write(f, f"{base}/Samples/{pack.name}/{f.name}")
        for page in sorted(WIKI.glob("*.md")):
            if not page.name.startswith("_"):
                z.write(page, f"{base}/LGPT-Guide/{page.name}")
        for image in sorted((WIKI / "images").iterdir()):
            z.write(image, f"{base}/LGPT-Guide/images/{image.name}")
    print(f"wrote {out} ({out.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
