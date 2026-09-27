#!/usr/bin/env python3
"""Mix report for the demo songs, from render_demos.py --keep-stems output.

Agents can't listen, so this answers the questions a mixing engineer would ask
by ear, per song:

- sections: level of every song row (does the drop hit harder than the intro,
  does the break breathe?) and the band balance of the loudest row;
- tracks: loudness and band profile of each stem;
- low end: how much of each track's low end lands on the kick hits (a bass
  playing on the kick blurs both; the kick track is found automatically);
- masking: pairs of tracks fighting in the same band at similar levels;
- reference: the loudest row's bands against a reference song (default the
  new Afterglow), so songs sit in the same league.

Usage (render first, then report; re-run the pair after changing presets):
    python tools/render_demos.py --no-build --keep-stems --artifacts sim-artifacts-demos
    python tools/demo_mix_report.py --artifacts sim-artifacts-demos
    python tools/demo_mix_report.py --artifacts sim-artifacts-demos --only neon_drive --reference afterglow
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import audio_report  # noqa: E402
import lgpt_composer  # noqa: E402

BANDS = audio_report.BANDS


def mono(path: Path) -> tuple[np.ndarray, int]:
    data, rate = audio_report.read_wav(path)
    return data.mean(axis=1), rate


def band_db(signal: np.ndarray, rate: int) -> dict[str, float]:
    """Absolute band levels (dB, mean power), so songs and tracks compare."""
    if len(signal) < 2048:
        return {name: -120.0 for name, _, _ in BANDS}
    n = 1 << int(np.log2(min(len(signal), rate * 4)))
    frames = [signal[i:i + n] * np.hanning(n) for i in range(0, len(signal) - n + 1, n)]
    spec = np.mean([np.abs(np.fft.rfft(f)) ** 2 for f in frames], axis=0) / n
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    out = {}
    for name, lo, hi in BANDS:
        mask = (freqs >= lo) & (freqs < hi)
        out[name] = round(10 * np.log10(spec[mask].sum() + 1e-12), 1)
    return out


def rms_db(signal: np.ndarray) -> float:
    return round(audio_report.db(float(np.sqrt(np.mean(signal ** 2))) if len(signal) else 0.0), 1)


def row_bounds(project: lgpt_composer.Project) -> list[tuple[int, float, float]]:
    """(row, start s, end s) of every song row, from the chains' lengths."""
    step = 60.0 / int(project.params["tempo"]) / 4
    out, t = [], 0.0
    for i, row in enumerate(project.song):
        chains = [c for c in row if c != 0xFF]
        if not chains:
            break
        bars = max(len(project._chains[c][0]) for c in chains)
        out.append((i, t, t + bars * 16 * step))
        t += bars * 16 * step
    return out


def lowpass(signal: np.ndarray, rate: int, cutoff: float = 250.0) -> np.ndarray:
    spec = np.fft.rfft(signal)
    spec[np.fft.rfftfreq(len(signal), 1.0 / rate) > cutoff] = 0
    return np.fft.irfft(spec, len(signal))


def onsets(signal: np.ndarray, rate: int) -> np.ndarray:
    """Sample positions where the track starts a hit (10 ms envelope rises)."""
    hop = rate // 100
    env = np.array([np.abs(signal[i:i + hop]).max() for i in range(0, len(signal) - hop, hop)])
    if env.max() <= 0:
        return np.array([], dtype=int)
    rise = np.diff(env, prepend=0)
    hits = np.where((rise > env.max() * 0.25) & (env > env.max() * 0.3))[0]
    keep, last = [], -100
    for h in hits:
        if h - last > 8:  # 80 ms apart at least
            keep.append(h)
        last = h
    return np.array(keep, dtype=int) * hop


def on_kick_share(track_low: np.ndarray, kick_hits: np.ndarray, rate: int) -> float:
    """Share of a track's low-end energy that sits in the 80 ms after kicks."""
    total = float(np.sum(track_low ** 2)) + 1e-18
    window = int(0.08 * rate)
    mask = np.zeros(len(track_low), dtype=bool)
    for h in kick_hits:
        mask[h:h + window] = True
    return float(np.sum(track_low[mask] ** 2)) / total


def kick_hz(signal: np.ndarray, hits: np.ndarray, rate: int) -> float:
    """Where the kick's body sits: the loudest frequency (25-200 Hz) in the
    150 ms after its hits (house and synthwave kicks want roughly 45-65 Hz)."""
    n = int(0.15 * rate)
    spec = np.zeros(n // 2 + 1)
    for h in hits[:64]:
        seg = signal[h:h + n]
        if len(seg) == n:
            spec += np.abs(np.fft.rfft(seg * np.hanning(n)))
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    band = (freqs > 25) & (freqs < 200)
    return float(freqs[band][np.argmax(spec[band])]) if spec.any() else 0.0


def find_kick(stems: dict[int, np.ndarray], rate: int) -> int | None:
    """The track whose low end is mostly short hits: a kick's energy sits
    right after its onsets, a bass holds its notes."""
    best, best_score = None, 0.0
    for t, s in stems.items():
        low = lowpass(s, rate, 150)
        hits = onsets(low, rate)
        if len(hits) < 8:
            continue
        sub = float(np.sum(low ** 2)) / (float(np.sum(s ** 2)) + 1e-18)
        score = sub * on_kick_share(low, hits, rate) ** 2
        if score > best_score:
            best, best_score = t, score
    return best


def song_report(name: str, artifacts: Path, reference: dict | None) -> dict:
    module = lgpt_composer.load_demos()[name]
    project = module.build()  # type: ignore[attr-defined]
    folder = f"lgpt_{project.name}"
    mix, rate = mono(artifacts / f"{folder}.wav")
    bounds = row_bounds(project)
    print(f"== {project.name}: {len(bounds)} rows, {bounds[-1][2]:.0f}s, tempo {project.params['tempo']}")
    peak = audio_report.db(float(np.abs(mix).max()))
    print(f"   mix peak {peak:.1f} dB  rms {rms_db(mix)} dB")
    rows = []
    for i, a, b in bounds:
        seg = mix[int(a * rate):int(b * rate)]
        rows.append((i, rms_db(seg), seg))
    loud = max(rows, key=lambda r: r[1])
    quiet = min(r[1] for r in rows)
    print("   rows (dB): " + " ".join(f"{i:02X}:{v:.0f}" for i, v, _ in rows)
          + f"   range {loud[1] - quiet:.1f} dB")
    drop = band_db(loud[2], rate)
    print(f"   loudest row {loud[0]:02X} bands: " + " ".join(f"{k} {v:.0f}" for k, v in drop.items()))
    if reference:
        print("   vs reference:          " + " ".join(f"{k} {drop[k] - reference[k]:+.0f}" for k in drop))

    stems = {}
    for t in range(8):
        path = artifacts / "stems" / f"{folder}-track{t + 1}.wav"
        if path.exists():
            s, _ = mono(path)
            stems[t] = s[:len(mix)]
    kick = find_kick(stems, rate)
    hits = onsets(lowpass(stems[kick], rate, 150), rate) if kick is not None else np.array([])
    lows = {t: lowpass(s, rate) for t, s in stems.items()}
    kick_low = rms_db(lows[kick]) if kick is not None else -120.0
    profiles = {}
    for t, s in stems.items():
        active = s[np.abs(s) > 1e-4]
        if len(active) < rate:
            continue
        # The track's bands in the loudest row, where masking matters most
        prof = band_db(s[int(bounds[loud[0]][1] * rate):int(bounds[loud[0]][2] * rate)], rate)
        profiles[t] = prof
        share = on_kick_share(lows[t], hits, rate) if kick is not None and t != kick else None
        low_level = rms_db(lows[t])
        flag = ""
        # Only a track with real low end competes with the kick (a snare on
        # 2 and 4 in four on the floor lands on kicks by design)
        if share is not None and share > 0.2 and low_level > kick_low - 10:
            flag = "  <- low end on the kicks"
        print(f"   track {t + 1}: rms {rms_db(active):6.1f}  low {low_level:6.1f}"
              + (f"  on-kick {share:4.0%}" if share is not None
                 else f"  (kick, body {kick_hz(s, hits, rate):.0f} Hz)" if t == kick else "")
              + "  bands " + " ".join(f"{v:.0f}" for v in prof.values()) + flag)
    # Masking: two tracks within 4 dB of each other in a band where both are
    # within 8 dB of the mix
    clashes = []
    for bi, (band, _, _) in enumerate(BANDS[:4]):
        for a in profiles:
            for b in profiles:
                if a >= b:
                    continue
                va, vb = profiles[a][band], profiles[b][band]
                if abs(va - vb) < 4 and min(va, vb) > drop[band] - 8:
                    clashes.append(f"{band}: tracks {a + 1}+{b + 1}")
    if clashes:
        print("   masking: " + ", ".join(clashes))
    return {"drop": drop, "rows": [(i, v) for i, v, _ in rows]}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--artifacts", type=Path, default=TOOLS.parent / "sim-artifacts-demos")
    ap.add_argument("--only", help="comma separated demo module names")
    ap.add_argument("--reference", default="afterglow", help="demo module to compare bands against")
    args = ap.parse_args()
    demos = lgpt_composer.load_demos()
    reference = None
    if args.reference:
        ref_wav = args.artifacts / f"lgpt_{demos[args.reference].build().name}.wav"  # type: ignore[attr-defined]
        if ref_wav.exists():
            reference = song_report(args.reference, args.artifacts, None)["drop"]
    wanted = args.only.split(",") if args.only else [n for n in demos if n != args.reference]
    for name in wanted:
        song_report(name, args.artifacts, reference)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
