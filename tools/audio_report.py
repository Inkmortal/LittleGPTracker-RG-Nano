#!/usr/bin/env python3
"""Objective listening report for simulator WAV captures and renders.

Agents cannot hear, so this turns a WAV into numbers and a picture:

- peak / RMS / crest factor / clipped-sample ratio / DC offset
- loudness per second (catches silent gaps and runaway levels)
- energy per frequency band (sub, bass, low-mid, mid, presence, air)
- spectral centroid (brightness) and a detected note histogram
- optional spectrogram + waveform PNG for visual inspection

Usage:
    python tools/audio_report.py capture.wav
    python tools/audio_report.py capture.wav --png report.png
    python tools/audio_report.py capture.wav --json report.json
    python tools/audio_report.py capture.wav --expect-no-clip --min-rms-db -30
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import wave
from pathlib import Path

import numpy as np

BANDS = [
    ("sub", 20, 60),
    ("bass", 60, 250),
    ("lowmid", 250, 800),
    ("mid", 800, 2500),
    ("presence", 2500, 6000),
    ("air", 6000, 16000),
]

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def read_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as wf:
        channels = wf.getnchannels()
        width = wf.getsampwidth()
        rate = wf.getframerate()
        frames = wf.readframes(wf.getnframes())
    if width == 2:
        data = np.frombuffer(frames, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        data = np.frombuffer(frames, dtype="<i4").astype(np.float64) / 2147483648.0
    elif width == 1:
        data = (np.frombuffer(frames, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    else:
        raise ValueError(f"unsupported sample width {width}")
    if channels > 1:
        data = data.reshape(-1, channels)
    else:
        data = data.reshape(-1, 1)
    return data, rate


def db(x: float) -> float:
    return 20.0 * math.log10(max(x, 1e-9))


def band_energy(mono: np.ndarray, rate: int) -> dict[str, float]:
    n = min(len(mono), rate * 30)
    if n < 1024:
        return {name: -120.0 for name, _, _ in BANDS}
    seg = mono[:n] * np.hanning(n)
    spec = np.abs(np.fft.rfft(seg)) ** 2
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    total = spec.sum() + 1e-18
    out = {}
    for name, lo, hi in BANDS:
        mask = (freqs >= lo) & (freqs < hi)
        out[name] = round(10.0 * math.log10(spec[mask].sum() / total + 1e-12), 1)
    return out


def spectral_centroid(mono: np.ndarray, rate: int) -> float:
    n = min(len(mono), rate * 30)
    if n < 1024:
        return 0.0
    spec = np.abs(np.fft.rfft(mono[:n] * np.hanning(n)))
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    return float((spec * freqs).sum() / (spec.sum() + 1e-18))


def note_histogram(mono: np.ndarray, rate: int, top: int = 8) -> list[tuple[str, float]]:
    """Chroma-style histogram of the strongest tonal peaks per 100ms frame."""
    frame = 4096
    hop = rate // 10
    counts = np.zeros(12)
    window = np.hanning(frame)
    freqs = np.fft.rfftfreq(frame, 1.0 / rate)
    valid = (freqs > 60) & (freqs < 2000)
    for start in range(0, max(0, len(mono) - frame), hop):
        spec = np.abs(np.fft.rfft(mono[start:start + frame] * window))
        spec[~valid] = 0
        if spec.max() <= 1e-4:
            continue
        idx = np.argsort(spec)[-4:]
        for i in idx:
            f = freqs[i]
            if f <= 0:
                continue
            midi = 69 + 12 * math.log2(f / 440.0)
            counts[int(round(midi)) % 12] += spec[i]
    if counts.sum() <= 0:
        return []
    counts /= counts.sum()
    order = np.argsort(counts)[::-1][:top]
    return [(NOTE_NAMES[i], round(float(counts[i]), 3)) for i in order if counts[i] > 0.01]


def analyze(path: Path) -> dict:
    data, rate = read_wav(path)
    mono = data.mean(axis=1)
    peak = float(np.abs(data).max()) if data.size else 0.0
    rms = float(np.sqrt(np.mean(data ** 2))) if data.size else 0.0
    clipped = float(np.mean(np.abs(data) >= 0.999)) if data.size else 0.0
    per_second = []
    for s in range(0, len(mono), rate):
        chunk = data[s:s + rate]
        if len(chunk) < rate // 4:
            break
        per_second.append(round(db(float(np.sqrt(np.mean(chunk ** 2)))), 1))
    silent_seconds = sum(1 for v in per_second if v < -60)
    stereo_width = 0.0
    if data.shape[1] == 2:
        mid = (data[:, 0] + data[:, 1]) * 0.5
        side = (data[:, 0] - data[:, 1]) * 0.5
        stereo_width = round(db(float(np.sqrt(np.mean(side ** 2)))) - db(float(np.sqrt(np.mean(mid ** 2)))), 1)
    return {
        "file": str(path),
        "seconds": round(len(mono) / rate, 2),
        "rate": rate,
        "channels": int(data.shape[1]),
        "peak_db": round(db(peak), 2),
        "rms_db": round(db(rms), 2),
        "crest_db": round(db(peak) - db(rms), 2),
        "clipped_ratio": round(clipped, 5),
        "dc_offset": round(float(mono.mean()), 5),
        "stereo_side_minus_mid_db": stereo_width,
        "rms_db_per_second": per_second,
        "silent_seconds": silent_seconds,
        "band_energy_db": band_energy(mono, rate),
        "centroid_hz": round(spectral_centroid(mono, rate), 1),
        "pitch_classes": note_histogram(mono, rate),
    }


def write_png(path: Path, out: Path) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    data, rate = read_wav(path)
    mono = data.mean(axis=1)
    fig, axes = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    t = np.arange(len(mono)) / rate
    axes[0].plot(t, data[:, 0], linewidth=0.3, color="#c040c0")
    axes[0].set_ylim(-1.05, 1.05)
    axes[0].axhline(0.999, color="red", linewidth=0.5)
    axes[0].axhline(-0.999, color="red", linewidth=0.5)
    axes[0].set_ylabel("amplitude")
    axes[0].set_title(path.name)
    axes[1].specgram(mono, NFFT=2048, Fs=rate, noverlap=1536, cmap="magma", vmin=-120)
    axes[1].set_ylim(20, 12000)
    axes[1].set_yscale("log")
    axes[1].set_ylabel("Hz")
    axes[1].set_xlabel("seconds")
    fig.tight_layout()
    fig.savefig(out, dpi=90)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("wav", type=Path)
    parser.add_argument("--png", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--expect-no-clip", action="store_true", help="fail if more than 0.01%% of samples clip")
    parser.add_argument("--min-rms-db", type=float, help="fail if overall RMS is below this")
    parser.add_argument("--max-silent-seconds", type=int, help="fail if more full seconds are silent")
    args = parser.parse_args()

    report = analyze(args.wav)
    print(json.dumps(report, indent=2))
    if args.json:
        args.json.write_text(json.dumps(report, indent=2))
    if args.png:
        write_png(args.wav, args.png)

    failures = []
    if args.expect_no_clip and report["clipped_ratio"] > 0.0001:
        failures.append(f"clipping {report['clipped_ratio']:.4%}")
    if args.min_rms_db is not None and report["rms_db"] < args.min_rms_db:
        failures.append(f"rms {report['rms_db']} dB < {args.min_rms_db}")
    if args.max_silent_seconds is not None and report["silent_seconds"] > args.max_silent_seconds:
        failures.append(f"{report['silent_seconds']} silent seconds")
    if failures:
        print("AUDIO REPORT FAIL: " + "; ".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
