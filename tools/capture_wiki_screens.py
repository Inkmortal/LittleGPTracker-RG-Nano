#!/usr/bin/env python3
"""Capture the RG Nano screens used by the wiki, straight from the simulator.

Runs projects/resources/RGNANO_SIM/wiki-shots-*.rgsim headless and muted,
then converts every 240x240 capture into a crisp 2x PNG (nearest neighbour)
in docs/rgnano-wiki/images/.

Usage:
    python tools/capture_wiki_screens.py
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
PROJECTS = ROOT / "projects"
SCRIPTS = PROJECTS / "resources" / "RGNANO_SIM"
IMAGES = ROOT / "docs" / "rgnano-wiki" / "images"
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def run(script: Path, reset: bool) -> None:
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
           str(TOOLS / "run-rgnano-sim.ps1"), "-Script", str(script), "-Mute"]
    if reset:
        cmd.append("-ResetLastProject")
    result = subprocess.run(cmd, cwd=ROOT, creationflags=NO_WINDOW)
    if result.returncode != 0:
        raise SystemExit(f"{script.name} failed; see projects/rgnano-sim.log")


def main() -> int:
    IMAGES.mkdir(parents=True, exist_ok=True)
    for old in ROOT.glob("wiki-*.bmp"):
        old.unlink()

    # Shoot the start screen with only the demo songs listed, not the
    # throwaway projects earlier sim runs leave behind
    tracks = ROOT / "rgnano-sim-data" / "tracks"
    parked = ROOT / "rgnano-sim-data" / "tracks-parked-for-wiki"
    if parked.exists():
        raise SystemExit(f"{parked} exists from an interrupted run; move it back to {tracks}")
    if tracks.exists():
        tracks.rename(parked)
    try:
        tracks.mkdir(parents=True)
        for demo_dir in (PROJECTS / "resources" / "demos").iterdir():
            if demo_dir.is_dir():
                shutil.copytree(demo_dir, tracks / demo_dir.name)
        run(SCRIPTS / "wiki-shots-new.rgsim", reset=True)
    finally:
        shutil.rmtree(tracks, ignore_errors=True)
        if parked.exists():
            parked.rename(tracks)

    # Demo screens: open Neon Drive through AUTO_LOAD_LAST
    demo = PROJECTS / "resources" / "demos" / "lgpt_NeonDrive"
    sim_demo = ROOT / "rgnano-sim-data" / "tracks" / "lgpt_NeonDrive"
    if sim_demo.exists():
        shutil.rmtree(sim_demo)
    shutil.copytree(demo, sim_demo)
    last = PROJECTS / "last_project"
    saved = last.read_text() if last.exists() else None
    last.write_text("./rgnano-sim-data/tracks/lgpt_NeonDrive")
    try:
        run(SCRIPTS / "wiki-shots-demo.rgsim", reset=False)
    finally:
        if saved is None:
            last.unlink(missing_ok=True)
        else:
            last.write_text(saved)

    count = 0
    for bmp in sorted(ROOT.glob("wiki-*.bmp")):
        img = Image.open(bmp).convert("RGB")
        img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
        img.save(IMAGES / (bmp.stem.replace("wiki-", "") + ".png"), optimize=True)
        bmp.unlink()
        count += 1
    print(f"wrote {count} screenshots to {IMAGES}")
    return 0 if count else 1


if __name__ == "__main__":
    sys.exit(main())
