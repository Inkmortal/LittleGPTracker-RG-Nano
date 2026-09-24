#!/usr/bin/env python3
"""Fetch acoustic single-note samples from VSCO 2 Community Edition (CC0).

VSCO 2 CE by Versilian Studios (https://github.com/sgossner/VSCO-2-CE) is
public domain (CC0 1.0). This picks a few notes per instrument - piano,
upright bass, strings, brass, harp, mallets, orchestral and hand percussion -
and, like tools/fetch_chinese_samples.py:

  1. downloads each WAV into exports/vsco-samples/source/ (cached, gitignored),
  2. decodes to 44.1 kHz mono, trims leading silence, cuts to a device-friendly
     length, fades the tail and normalizes the peak to -1 dBFS,
  3. writes projects/resources/samples/<pack>/<slug>.wav,
  4. detects the root pitch of pitched samples (LGPT naming, C3 = MIDI 60) and
     records it in projects/resources/samples/<pack>/samples.json, which the
     demo songs read,
  5. writes a CREDITS.md per pack.

Usage:  python tools/fetch_vsco_samples.py [--refresh] [slug ...]
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path

import numpy as np

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from fetch_chinese_samples import (SR, count_onsets, decode, midi_of,  # noqa: E402
                                   pitch_track, read_wav, trim_onset, write_wav)
from lgpt_composer import note_name  # noqa: E402

SOURCE_DIR = ROOT / "exports" / "vsco-samples" / "source"
SAMPLES = ROOT / "projects" / "resources" / "samples"
RAW = "https://raw.githubusercontent.com/sgossner/VSCO-2-CE/master/"
PEAK_DBFS = -1.0
USER_AGENT = "LittleGPTracker-RG-Nano-sample-fetch/1.0"


@dataclass
class Source:
    pack: str        # output folder under projects/resources/samples
    slug: str        # output file name
    path: str        # path inside the VSCO-2-CE repository
    instrument: str
    length: float    # seconds kept
    pitched: bool = True
    kind: str = "sustain"  # "pluck" or "sustain": where pitch tracking looks


P = "Keys/Upright Piano/"
SOURCES = [
    # keys: upright piano (key numbers from MappingChart.txt), harp, mallets
    Source("keys", "piano-low", P + "Player_dyn2_rr1_012.wav", "Upright piano", 3.0, kind="pluck"),
    Source("keys", "piano-mid", P + "Player_dyn2_rr1_018.wav", "Upright piano", 3.0, kind="pluck"),
    Source("keys", "piano-high", P + "Player_dyn2_rr1_024.wav", "Upright piano", 2.5, kind="pluck"),
    Source("keys", "piano-soft", P + "Player_dyn1_rr1_018.wav", "Upright piano, soft", 3.0, kind="pluck"),
    Source("keys", "harp-low", "Strings/Harp/KSHarp_C3_mf.wav", "Harp", 2.5, kind="pluck"),
    Source("keys", "harp-high", "Strings/Harp/KSHarp_C5_mf.wav", "Harp", 2.0, kind="pluck"),
    Source("keys", "glock", "Percussion/Glock/glock_medium_C5.wav", "Glockenspiel", 2.0, kind="pluck"),
    Source("keys", "marimba", "Percussion/Marimba/Marimba_hit_Outrigger_C4_loud_01.wav", "Marimba", 1.5,
           kind="pluck"),
    # strings
    Source("strings", "violins-low", "Strings/Violin Section/susVib/VlnEns_susVib_A3_v2.wav",
           "Violin section, sustain", 3.0),
    Source("strings", "violins-high", "Strings/Violin Section/susVib/VlnEns_susVib_E4_v2.wav",
           "Violin section, sustain", 3.0),
    Source("strings", "cellos", "Strings/Cello Section/susvib/susvib_C3_v3_1.wav", "Cello section, sustain", 3.0),
    Source("strings", "cellos-low", "Strings/Cello Section/susvib/susvib_G1_v3_1.wav", "Cello section, sustain",
           3.0),
    Source("strings", "violin-pizz", "Strings/Violin Section/Pizz/VlnEns_Pizz_A3_v2_rr1.wav",
           "Violin section, pizzicato", 1.0, kind="pluck"),
    Source("strings", "cello-pizz", "Strings/Cello Section/pizzT/pizzT_C3_v2_RR1.wav", "Cello section, pizzicato",
           1.5, kind="pluck"),
    # brass
    Source("brass", "horn", "Brass/F Horn/sus/MOHorn_sus_F2_v3_1.wav", "French horn", 3.0),
    Source("brass", "trumpet", "Brass/Trumpet/sus/Sum_SHTrumpet_sus_A#3_v3_rr1.wav", "Trumpet", 2.5),
    Source("brass", "trombone", "Brass/Tenor Trombone/sus/tenortbn_sus_D2_v3_1.wav", "Tenor trombone", 2.5),
    # bass
    Source("bass", "upright-low", "Strings/Solo Contrabass/Pizz/BKCtbss_Pizz_E1_v1_rr1.wav",
           "Upright bass, pizzicato", 2.0, kind="pluck"),
    Source("bass", "upright-high", "Strings/Solo Contrabass/Pizz/BKCtbss_Pizz_A1_v1_rr1.wav",
           "Upright bass, pizzicato", 2.0, kind="pluck"),
    # percussion: orchestral and hand drums (unpitched unless noted)
    Source("percussion", "timpani", "Percussion/Timpani/Timpani1_Hit_v3_rr1_Sum.wav", "Timpani", 3.0,
           kind="pluck"),
    Source("percussion", "timpani-2", "Percussion/Timpani/Timpani2_Hit_v3_rr1_Sum.wav", "Timpani", 3.0,
           kind="pluck"),
    Source("percussion", "bass-drum", "VSCO 1 Percussion/drums/bass/bdrum_ff_1.wav", "Concert bass drum", 2.5,
           pitched=False),
    Source("percussion", "snare", "VSCO 1 Percussion/drums/snare/drum1/snare1_f_1.wav", "Snare drum", 1.0,
           pitched=False),
    Source("percussion", "snare-old", "VSCO 1 Percussion/drums/snare/OldSnare/snare_f.wav", "Old snare drum",
           0.6, pitched=False),
    Source("percussion", "crash", "VSCO 1 Percussion/varMetal/Cymbals/clash/crash_hit_ff_tight.wav",
           "Crash cymbals", 3.0, pitched=False),
    Source("percussion", "cymbal", "VSCO 1 Percussion/varMetal/Cymbals/susp/susp_hit_metal_1.wav",
           "Suspended cymbal", 3.0, pitched=False),
    Source("percussion", "shaker", "VSCO 1 Percussion/varWood/Camo's Shaker/shake3.wav", "Shaker", 0.5,
           pitched=False),
    Source("percussion", "bongo-high", "VSCO 1 Percussion/drums/other/Bongos/HighBongo1.wav", "Bongo, high", 0.8,
           pitched=False),
    Source("percussion", "bongo-low", "VSCO 1 Percussion/drums/other/Bongos/LowBongo1.wav", "Bongo, low", 0.8,
           pitched=False),
    Source("percussion", "conga-high", "VSCO 1 Percussion/drums/other/ethnic/congo/open/ethnicHighOpen_hit_f_1.wav",
           "Conga, high", 0.8, pitched=False),
    Source("percussion", "conga-low", "VSCO 1 Percussion/drums/other/ethnic/congo/open/ethnicLowOpen_hit_f_1.wav",
           "Conga, low", 0.8, pitched=False),
    Source("percussion", "triangle", "VSCO 1 Percussion/varMetal/triangle/1/triangle1_hit_mp_muted1.wav",
           "Triangle, muted", 1.0, pitched=False),
    Source("percussion", "claves", "VSCO 1 Percussion/varWood/claves_ff.wav", "Claves", 0.5, pitched=False),
]


def download(src: Source, refresh: bool) -> Path:
    dest = SOURCE_DIR / (src.path.replace("/", "__").replace(" ", "_"))
    if dest.exists() and dest.stat().st_size > 1024 and not refresh:
        return dest
    url = RAW + urllib.parse.quote(src.path)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    last: Exception | None = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=120) as resp:
                data = resp.read()
            break
        except Exception as exc:  # network hiccup: retry, then fail loudly
            last = exc
            time.sleep(2 * (attempt + 1))
    else:
        raise RuntimeError(f"download failed for {src.slug}: {last}")
    if len(data) < 1024:
        raise RuntimeError(f"download too small for {src.slug}: {len(data)} bytes")
    dest.write_bytes(data)
    return dest


def process(src: Source, raw: np.ndarray) -> np.ndarray:
    seg = trim_onset(raw[: int((src.length + 1.5) * SR)])[: int(src.length * SR)]
    seg = seg - seg.mean()
    n = len(seg)
    fade_in = int(0.001 * SR)
    seg[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    fade_out = int(min(max(0.3 * n, 0.05 * SR), 0.6 * SR))
    seg[n - fade_out:] *= 0.5 * (1 + np.cos(np.linspace(0, math.pi, fade_out)))
    seg *= 10 ** (PEAK_DBFS / 20) / np.abs(seg).max()
    return seg


def credits_md(pack: str, rows: list[dict]) -> str:
    lines = [
        f"# {pack.capitalize()} samples - credits",
        "",
        "From **VSCO 2 Community Edition** by Versilian Studios",
        "(https://github.com/sgossner/VSCO-2-CE), released under",
        "[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (public domain).",
        "The upright piano was sampled by Simon Dalzell of Ivy Audio; credit to both is encouraged.",
        "",
        "Fetched and processed by `tools/fetch_vsco_samples.py` (44.1 kHz mono 16-bit, trimmed,",
        "tail-faded, peak-normalized to -1 dBFS). Root notes use LGPT naming (C3 = MIDI 60).",
        "",
        "| File | Instrument | Root (LGPT) | Source file |",
        "|------|------------|-------------|-------------|",
    ]
    for r in rows:
        s: Source = r["src"]
        lines.append(f"| `{s.slug}.wav` | {s.instrument} | {r['root']} | `{s.path}` |")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("slugs", nargs="*", help="only process these slugs")
    ap.add_argument("--refresh", action="store_true", help="re-download cached sources")
    args = ap.parse_args()

    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    wanted = [s for s in SOURCES if not args.slugs or s.slug in args.slugs]
    unknown = set(args.slugs) - {s.slug for s in SOURCES}
    if unknown:
        raise SystemExit(f"unknown slug(s): {', '.join(sorted(unknown))}")

    by_pack: dict[str, list[dict]] = {}
    print(f"{'pack':10} {'slug':13} {'dur':>5} {'peak':>6} {'rms':>6} {'on':>2}  {'root':6} {'Hz':>7} {'cents':>6}")
    for src in wanted:
        raw = decode(download(src, args.refresh))
        out = process(src, raw)
        folder = SAMPLES / src.pack
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / f"{src.slug}.wav"
        write_wav(path, out)
        x = read_wav(path)
        dur = len(x) / SR
        peak_db = 20 * math.log10(np.abs(x).max())
        rms_db = 20 * math.log10(np.sqrt(np.mean(x ** 2)))
        onsets = count_onsets(x)
        root, hz, cents = "C3", 0.0, 0.0
        if src.pitched:
            hz, _, _ = pitch_track(x, src.kind)
            if hz <= 0:
                raise RuntimeError(f"{src.slug}: no stable pitch found")
            midi, cents = midi_of(hz)
            root = note_name(midi)
        if rms_db < -45:
            raise RuntimeError(f"{src.slug}: nearly silent ({rms_db:.1f} dBFS RMS)")
        print(f"{src.pack:10} {src.slug:13} {dur:5.2f} {peak_db:6.1f} {rms_db:6.1f} {onsets:2d}  "
              f"{root if src.pitched else '-':6} {hz:7.1f} {cents:+6.0f}")
        by_pack.setdefault(src.pack, []).append(
            {"src": src, "root": root if src.pitched else "unpitched", "root_note": root})

    for pack, rows in by_pack.items():
        folder = SAMPLES / pack
        manifest_path = folder / "samples.json"
        manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
        for r in rows:
            manifest[r["src"].slug] = {"root": r["root_note"], "instrument": r["src"].instrument,
                                      "pitched": r["src"].pitched}
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        if not args.slugs:
            (folder / "CREDITS.md").write_text(credits_md(pack, rows), encoding="utf-8")
    print(f"wrote {sum(len(r) for r in by_pack.values())} samples into {len(by_pack)} packs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
