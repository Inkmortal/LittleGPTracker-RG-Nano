#!/usr/bin/env python3
"""Bounce a short, idiomatic phrase for each Chinese instrument preset
(docs/rgnano-wiki/Chinese-Instruments.md) through the real app, so they can
be listened to outside the tracker.

Each preset gets its own tiny project (one instrument, a few bars using the
playing techniques the guide teaches: PTCH bends/slides, LEGA glides, VIBR,
RTRG tremolo), rendered with the same Project -> Render: Stereo pipeline as
tools/render_demos.py. Unlike render_demos.py, these are NOT written into
projects/resources/demos (they are not shipped demo songs) - they go
straight to the given output folder as WAV files.

Usage:
    python tools/render_chinese_presets.py
    python tools/render_chinese_presets.py --only guzheng,erhu
    python tools/render_chinese_presets.py --out "C:\\Users\\me\\Music\\LGPT\\Chinese"
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
PROJECTS = ROOT / "projects"
SIM_TRACKS = ROOT / "rgnano-sim-data" / "tracks"
DEFAULT_OUT = Path.home() / "Music" / "LGPT" / "Chinese"

sys.path.insert(0, str(TOOLS))
import audio_report  # noqa: E402
import lgpt_composer  # noqa: E402
import render_demos  # noqa: E402

from lgpt_composer import Phrase, Project, note  # noqa: E402

MELODY_TEMPO = 88
PERC_TEMPO = 104


def bar(text: str) -> Phrase:
    return Phrase.parse(text, 0)


def slide_in(p: Phrase, step: int, semitones_below: int, ticks: int = 0xF9) -> Phrase:
    """LEGA aabb: slide up from `semitones_below` semitones under the step's
    note, arriving over `ticks` (see Chinese-Instruments.md)."""
    return p.command(step, "LEGA", (semitones_below << 8) | ticks)


def vibrato(p: Phrase, step: int, amount: int = 0x56) -> Phrase:
    return p.command(step, "VIBR", amount)


def bend_up(p: Phrase, step: int, semitones: int = 2) -> Phrase:
    """PTCH aabb bends up `semitones` (an yin, the guzheng press-bend)."""
    return p.command(step, "PTCH", (semitones << 8) | 0x02)


def tremolo(p: Phrase, step: int, rate: int = 0x02) -> Phrase:
    return p.command(step, "RTRG", rate)


def kill(p: Phrase, step: int) -> Phrase:
    return p.command(step, "KILL", 0)


# --- one build() per preset: (name, tempo, key/scale, chain of bars) -------

def build_guzheng() -> Project:
    p = Project("guzheng", MELODY_TEMPO)
    p.key("D", "Gong (Chinese)")
    p.synth(0, "guzheng")
    b1 = bar("D3 . A3 . D4 . . F#4 | . . A3 . . . . .")
    bend_up(b1, 2, 2)
    b2 = bar("G3 A3 D4 E4 F#4 A4 . . | D4 . A3 . D3 . . .")
    b3 = bar("A3 . D4 . F#4 . A4 . | G4 F#4 D4 A3 . . . .")
    kill(b3, 15)
    b4 = bar(". . . . D3 . A3 . | D4 F#4 A4 D5 . . . .")
    kill(b4, 15)
    ch = p.chain([b1, b2, b3, b4])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_pipa() -> Project:
    p = Project("pipa", MELODY_TEMPO)
    p.key("D", "Gong (Chinese)")
    p.synth(0, "pipa")
    b1 = bar("E4 D4 A3 . G3 A3 D4 . | E4 . D4 . A3 . . .")
    tremolo(b1, 8, 2)
    b2 = bar("D4 . . . F#4 E4 D4 . | A3 . D4 . . . . .")
    tremolo(b2, 0, 2)
    kill(b2, 15)
    ch = p.chain([b1, b2])
    p.row(0, [ch, ch, None, None, None, None, None, None])
    return p


def build_yangqin() -> Project:
    p = Project("yangqin", MELODY_TEMPO + 8)
    p.key("D", "Gong (Chinese)")
    p.synth(0, "yangqin")
    b1 = bar("D4 E4 F#4 A4 D4 E4 F#4 A4 | G4 F#4 E4 D4 A3 D4 F#4 A4")
    tremolo(b1, 15, 3)
    b2 = bar("A4 F#4 D4 A3 D4 F#4 A4 D5 | A4 F#4 D4 A3 . . . .")
    kill(b2, 15)
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_guqin() -> Project:
    p = Project("guqin", 66)
    p.key("D", "Yu (Chinese)")
    p.synth(0, "guqin")
    b1 = bar(". . D2 . . . . . | . . . . A2 . . .")
    slide_in(b1, 12, 5)
    b2 = bar(". . D3 . . . . . | . . . . F3 . . .")
    slide_in(b2, 12, 3)
    b3 = bar(". . A2 . . . . . | . . . . D3 . . .")
    kill(b3, 12)
    ch = p.chain([b1, b2, b3])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_erhu() -> Project:
    p = Project("erhu", MELODY_TEMPO - 4)
    p.key("D", "Yu (Chinese)")
    p.synth(0, "erhu")
    b1 = bar("D4 . . . E4 . . . | F4 . . . E4 . . .")
    slide_in(b1, 0, 7)
    vibrato(b1, 8, 0x56)
    b2 = bar("D4 . . A3 . . . . | C4 . D4 . . . . .")
    slide_in(b2, 3, 9)
    vibrato(b2, 11, 0x60)
    kill(b2, 15)
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_dizi() -> Project:
    p = Project("dizi", MELODY_TEMPO + 12)
    p.key("D", "Gong (Chinese)")
    p.synth(0, "dizi")
    b1 = bar("D4 E4 F#4 A4 D5 A4 F#4 E4 | D4 . F#4 . A4 . D5 .")
    b1.command(0, "PTCH", 0x0200)
    b1.command(2, "PTCH", 0x0200)
    b2 = bar("A4 F#4 D4 E4 F#4 A4 D5 . | A4 F#4 D4 . . . . .")
    kill(b2, 15)
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_xiao() -> Project:
    p = Project("xiao", 60)
    p.key("D", "Yu (Chinese)")
    p.synth(0, "xiao")
    b1 = bar(". D4 . . . . . . | . A3 . . . . . .")
    b2 = bar(". C4 . . . . . . | . D4 . . . . . .")
    kill(b2, 9)
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_sheng() -> Project:
    p = Project("sheng", 72)
    p.key("D", "Gong (Chinese)")
    p.synth(0, "sheng")
    b1 = bar("D4 . . . . . . . | A3 . . . . . . .")
    b2 = bar("G3 . . . . . . . | D4 . . . . . . .")
    kill(b2, 15)
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_suona() -> Project:
    p = Project("suona", MELODY_TEMPO + 20)
    p.key("D", "Zhi (Chinese)")
    p.synth(0, "suona")
    b1 = bar("D4 . E4 F#4 A4 . A4 . | G4 F#4 E4 D4 . . . .")
    slide_in(b1, 0, 8)
    vibrato(b1, 4, 0x60)
    b2 = bar("A4 . . F#4 . . D4 . | E4 F#4 D4 . . . . .")
    kill(b2, 12)
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_bianzhong() -> Project:
    p = Project("bianzhong", 96)
    p.key("D", "Gong (Chinese)")
    p.synth(0, "bianzhong")
    b1 = bar("D4 . A3 . D4 . F#4 . | A4 . F#4 . D4 . . .")
    b2 = bar("G3 . A3 . D4 . . . | . . . . . . . .")
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_big_gong() -> Project:
    p = Project("big_gong", 72)
    p.synth(0, "big gong")
    b1 = bar("D3 . . . . . . . | . . . . . . . .")
    b2 = bar(". . . . D3 . . . | . . . . . . . .")
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_opera_gong() -> Project:
    p = Project("opera_gong", 108)
    p.synth(0, "opera gong")
    b1 = bar("D4 . . D4 . . D4 . | . D4 . . D4 . . .")
    b2 = bar("D4 . . . D4 . D4 . | . . D4 . . . . .")
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_woodblock() -> Project:
    p = Project("woodblock", 108)
    p.synth(0, "woodblock")
    b1 = bar("D4 . D4 . D4 . D4 . | D4 . D4 . D4 . D4 .")
    b2 = bar("D4 . D4 . D4 D4 . D4 | . D4 . D4 . D4 D4 .")
    ch = p.chain([b1, b1, b2, b1])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_bangzi() -> Project:
    p = Project("bangzi", 112)
    p.synth(0, "bangzi")
    b1 = bar("D5 . . D5 . D5 . . | D5 . . D5 . . D5 .")
    b2 = bar("D5 . D5 . . D5 . D5 | . . D5 . D5 . . D5")
    ch = p.chain([b1, b2, b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_dagu() -> Project:
    p = Project("dagu", 100)
    p.synth(0, "dagu")
    b1 = bar("C3 . . C3 . . C3 . | . C3 . . C3 . C3 .")
    b2 = bar("C3 . C3 . . C3 . C3 | . . C3 C3 . C3 . .")
    ch = p.chain([b1, b1, b2, b1])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_tanggu() -> Project:
    p = Project("tanggu", 108)
    p.synth(0, "tanggu")
    b1 = bar("C3 . C3 C3 . C3 . C3 | C3 . C3 . C3 C3 . C3")
    b2 = bar("C3 C3 . C3 . C3 C3 . | C3 . C3 C3 . C3 . C3")
    ch = p.chain([b1, b2, b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


def build_bo_cymbal() -> Project:
    p = Project("bo_cymbal", 100)
    p.synth(0, "bo cymbal")
    b1 = bar("C3 . . . . . . . | . . . . C3 . . .")
    b2 = bar(". . . . . . . . | C3 . . . . . . .")
    ch = p.chain([b1, b2])
    p.row(0, [ch, None, None, None, None, None, None, None])
    return p


BUILDERS = {
    "guzheng": build_guzheng,
    "pipa": build_pipa,
    "yangqin": build_yangqin,
    "guqin": build_guqin,
    "erhu": build_erhu,
    "dizi": build_dizi,
    "xiao": build_xiao,
    "sheng": build_sheng,
    "suona": build_suona,
    "bianzhong": build_bianzhong,
    "big_gong": build_big_gong,
    "opera_gong": build_opera_gong,
    "woodblock": build_woodblock,
    "bangzi": build_bangzi,
    "dagu": build_dagu,
    "tanggu": build_tanggu,
    "bo_cymbal": build_bo_cymbal,
}


def render_one(name: str, project: "Project", out_dir: Path, tail: float) -> Path:
    """Save, point last_project at it, render Stereo, trim and copy the WAV
    to out_dir/<name>.wav. Never touches projects/resources/demos."""
    folder = project.save(SIM_TRACKS)
    mixdown = folder / "mixdown.wav"
    (PROJECTS / "last_project").write_text(f"./rgnano-sim-data/tracks/{folder.name}")
    seconds = project.length_seconds()
    wait_ms = int(seconds * 1000 + tail * 1000 + 1500)
    code = render_demos.run_render(name, 1, wait_ms, False)
    if code != 0 or not mixdown.exists():
        raise RuntimeError(f"{name}: render failed (exit {code}); see {render_demos.LOG}")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_wav = out_dir / f"{name}.wav"
    render_demos.trim_wav(mixdown, out_wav, seconds + tail)
    mixdown.unlink()
    shutil.rmtree(folder, ignore_errors=True)
    return out_wav


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", help="comma separated preset names (BUILDERS keys)")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--tail", type=float, default=1.5, help="seconds kept after the phrase ends (let long presets ring)")
    args = parser.parse_args()

    if not args.no_build:
        render_demos.build_sim()

    wanted = set(args.only.split(",")) if args.only else set(BUILDERS)
    unknown = wanted - set(BUILDERS)
    if unknown:
        print(f"unknown preset(s): {', '.join(sorted(unknown))}", file=sys.stderr)
        return 1

    last_project = PROJECTS / "last_project"
    saved_last = last_project.read_text() if last_project.exists() else None
    failures = []
    try:
        for name in BUILDERS:
            if name not in wanted:
                continue
            project = BUILDERS[name]()
            print(project.stats(), flush=True)
            # Gongs/bells ring after the hit: give the tail room to cover it
            # (still well under the 20 s die-away every phys preset must meet)
            tail = 12.0 if name == "opera_gong" else 5.0 if name in ("big_gong", "bianzhong") else args.tail
            wav = render_one(name, project, args.out, tail)
            report = audio_report.analyze(wav)
            print(f"  -> {wav}  peak {report['peak_db']} dB  rms {report['rms_db']} dB  "
                  f"silent {report['silent_seconds']}s  clipped {report['clipped_ratio']:.4%}", flush=True)
            if report["peak_db"] < -60.0:
                failures.append(f"{name}: silent")
            if report["clipped_ratio"] > 0.001:
                failures.append(f"{name}: clips")
    finally:
        if saved_last is None:
            last_project.unlink(missing_ok=True)
        else:
            last_project.write_text(saved_last)

    if failures:
        print("FAIL: " + ", ".join(failures), file=sys.stderr)
        return 1
    print(f"wrote {len(wanted)} preset demo(s) to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
