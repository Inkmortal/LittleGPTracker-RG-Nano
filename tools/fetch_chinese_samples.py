#!/usr/bin/env python3
"""Fetch a small, redistributable set of single-note Chinese instrument samples.

Every source is a single note / single hit (or one isolated note cut from a
solo recording) published on Freesound under CC0 or CC BY. The script:

  1. downloads each Freesound HQ preview (no login needed) into
     exports/chinese-samples/source/ (cached, gitignored),
  2. decodes it with ffmpeg to 44.1 kHz mono float,
  3. seeks to the note, trims leading silence, cuts to a device-friendly length,
     fades the tail, normalizes the peak to -1 dBFS,
  4. writes 16-bit mono WAVs to projects/resources/samples/chinese/<slug>.wav,
  5. detects the root pitch of pitched samples (autocorrelation over the steady
     part) and reports it in LGPT note naming (C3 = MIDI 60, see
     tools/lgpt_composer.py note()/note_name()),
  6. writes projects/resources/samples/chinese/CREDITS.md.

Usage:  python tools/fetch_chinese_samples.py [--refresh] [slug ...]
"""

from __future__ import annotations

import argparse
import math
import shutil
import subprocess
import sys
import time
import urllib.request
import wave
from dataclasses import dataclass
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from lgpt_composer import note_name  # noqa: E402  (LGPT octave naming, C3 = 60)

SOURCE_DIR = ROOT / "exports" / "chinese-samples" / "source"
OUT_DIR = ROOT / "projects" / "resources" / "samples" / "chinese"
SR = 44100
PEAK_DBFS = -1.0
USER_AGENT = "LittleGPTracker-RG-Nano-sample-fetch/1.0"

CC0 = ("CC0 1.0", "https://creativecommons.org/publicdomain/zero/1.0/")
CC_BY_4 = ("CC BY 4.0", "https://creativecommons.org/licenses/by/4.0/")

# Max lengths per kind (seconds) - keeps RAM use on the RG Nano low.
MAX_LEN = {"pluck": 2.5, "hit": 2.5, "sustain": 3.0, "gong": 5.0}


@dataclass(frozen=True)
class Source:
    slug: str
    instrument: str
    freesound_id: int
    title: str           # title on the Freesound page (identity check)
    author: str
    license: tuple[str, str]
    preview_url: str     # Freesound HQ mp3 preview
    start: float         # seconds into the source where the note begins (approx.)
    length: float        # seconds kept after the trimmed onset
    kind: str            # pluck | hit | sustain | gong
    pitched: bool
    notes: str = ""

    @property
    def page_url(self) -> str:
        return f"https://freesound.org/people/{self.author}/sounds/{self.freesound_id}/"


SOURCES: list[Source] = [
    Source("guzheng", "Guzheng (plucked zither)", 847157,
           "GUZHENG - instrument- Single Note - Sound", "nanliu_music", CC0,
           "https://cdn.freesound.org/previews/847/847157_18537710-hq.mp3",
           0.0, 2.5, "pluck", True,
           "First of the two notes in the file (the second, lower note starts at 7.4 s)."),
    Source("pipa", "Pipa (plucked lute)", 162085,
           "Pipa-3.wav", "xserra", CC_BY_4,
           "https://cdn.freesound.org/previews/162/162085_43-hq.mp3",
           15.95, 0.9, "pluck", True,
           "One isolated note cut from a pipa graduation recital at the Central Conservatory "
           "of Music, Beijing (recorded by Xavier Serra, 2012). Cut before the next note enters; "
           "slight room reverb."),
    Source("pipa-low", "Pipa (plucked lute), low note", 162085,
           "Pipa-3.wav", "xserra", CC_BY_4,
           "https://cdn.freesound.org/previews/162/162085_43-hq.mp3",
           24.98, 1.0, "pluck", True,
           "Second isolated note cut from the same pipa recital recording."),
    Source("erhu", "Erhu (bowed two-string fiddle)", 467483,
           "005-Erhu-D5.wav", "tarane468", CC0,
           "https://cdn.freesound.org/previews/467/467483_5876986-hq.mp3",
           0.45, 3.0, "sustain", True,
           "Second (long) bow stroke of the file."),
    Source("erhu-low", "Erhu (bowed two-string fiddle), low string", 569841,
           "#1 Erhu D3.wav", "tarane468", CC0,
           "https://cdn.freesound.org/previews/569/569841_5876986-hq.mp3",
           0.0, 3.0, "sustain", True,
           "Soft-attack sustained note."),
    Source("dizi", "Dizi (bamboo flute)", 384945,
           "Flute Dizi C_076_E6.wav", "Hypnotriod", CC0,
           "https://cdn.freesound.org/previews/384/384945_5093019-hq.mp3",
           0.0, 3.0, "sustain", True,
           "From the Hypnotriod 'Flute Dizi C' pack."),
    Source("dizi-a", "Dizi (bamboo flute)", 384960,
           "Flute Dizi C_081_A6.wav", "Hypnotriod", CC0,
           "https://cdn.freesound.org/previews/384/384960_5093019-hq.mp3",
           0.0, 3.0, "sustain", True,
           "From the Hypnotriod 'Flute Dizi C' pack; in key for A minor pentatonic."),
    Source("gong", "Daluo (Beijing opera large gong)", 222233,
           "daluo_46", "ajaysm", CC_BY_4,
           "https://cdn.freesound.org/previews/222/222233_2385996-hq.mp3",
           0.6, 5.0, "gong", False,
           "QMUL Beijing Opera Percussion dataset (ICASSP 2014), played by Ying Wan, "
           "London Jing Kun Opera Association."),
    Source("drum", "Dagu (Chinese bass drum), centre stroke", 856549,
           "Chinese Bass Drum Dagu - Center Stroke on Drumhead - Loud", "sazanami12", CC0,
           "https://cdn.freesound.org/previews/856/856549_17110092-hq.mp3",
           0.0, 2.5, "hit", False),
    Source("drum-rim", "Dagu (Chinese bass drum), rim shot", 856559,
           "Chinese Bass Drum Dagu - Rim Shot from Both Sides - Loud", "sazanami12", CC0,
           "https://cdn.freesound.org/previews/856/856559_17110092-hq.mp3",
           0.0, 1.5, "hit", False),
    Source("woodblock", "Temple block / muyu (hard wood mallet)", 863784,
           "Temple Blocks (5pcs) - Hard Wood Mallets", "sazanami12", CC0,
           "https://cdn.freesound.org/previews/863/863784_17110092-hq.mp3",
           3.70, 0.6, "hit", False,
           "Fourth (lowest) of the five blocks struck in the file."),
    Source("bangu", "Bangu / danpigu (Beijing opera clapper drum)", 222189,
           "bangu_59", "ajaysm", CC_BY_4,
           "https://cdn.freesound.org/previews/222/222189_2385996-hq.mp3",
           0.0, 0.5, "hit", False,
           "QMUL Beijing Opera Percussion dataset. Dry, woody crack."),
    Source("xiaoluo", "Xiaoluo (Beijing opera small gong)", 222361,
           "xiaoluo_65", "ajaysm", CC_BY_4,
           "https://cdn.freesound.org/previews/222/222361_2385996-hq.mp3",
           0.0, 1.5, "hit", False,
           "QMUL Beijing Opera Percussion dataset. Characteristic rising-pitch opera gong."),
    Source("naobo", "Naobo / qibo (Beijing opera cymbals)", 222298,
           "naobo_62", "ajaysm", CC_BY_4,
           "https://cdn.freesound.org/previews/222/222298_2385996-hq.mp3",
           0.0, 1.5, "hit", False,
           "QMUL Beijing Opera Percussion dataset."),
]


# --------------------------------------------------------------------------- io

def ffmpeg_exe() -> str:
    found = shutil.which("ffmpeg")
    if found:
        return found
    fallback = Path("C:/Users/danhc/Documents/Programs/ffmpeg/bin/ffmpeg.exe")
    if fallback.exists():
        return str(fallback)
    raise RuntimeError("ffmpeg not found on PATH")


def download(src: Source, refresh: bool) -> Path:
    dest = SOURCE_DIR / f"{src.freesound_id}_{src.slug}.mp3"
    if dest.exists() and dest.stat().st_size > 1024 and not refresh:
        return dest
    req = urllib.request.Request(src.preview_url, headers={"User-Agent": USER_AGENT})
    last: Exception | None = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
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


def decode(path: Path) -> np.ndarray:
    raw = subprocess.run(
        [ffmpeg_exe(), "-v", "error", "-i", str(path), "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"],
        capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.float32).astype(np.float64)


def write_wav(path: Path, x: np.ndarray) -> None:
    pcm = np.clip(np.round(x * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def read_wav(path: Path) -> np.ndarray:
    with wave.open(str(path), "rb") as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == SR, path
        return np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(np.float64) / 32768.0


# ---------------------------------------------------------------- processing

def trim_onset(x: np.ndarray, rel_db: float = -36.0, preroll_ms: float = 3.0) -> np.ndarray:
    """Drop leading silence: start a few ms before the envelope first crosses rel_db re peak."""
    peak = np.abs(x).max()
    if peak <= 0:
        raise RuntimeError("silent source segment")
    thr = peak * 10 ** (rel_db / 20)
    idx = int(np.argmax(np.abs(x) >= thr))
    start = max(0, idx - int(SR * preroll_ms / 1000))
    return x[start:]


def process(src: Source, raw: np.ndarray) -> np.ndarray:
    limit = MAX_LEN[src.kind]
    if src.length > limit:
        raise ValueError(f"{src.slug}: length {src.length}s exceeds {limit}s limit for {src.kind}")
    seg = raw[int(src.start * SR):]
    # search for the onset in a short window only, so a later louder note can't be picked
    window = seg[: int((src.length + 1.0) * SR)]
    seg = trim_onset(window)[: int(src.length * SR)]
    seg = seg - seg.mean()
    n = len(seg)
    fade_in = int(0.001 * SR)
    seg[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    # tail fade: last 30% (bounded 50..600 ms), raised-cosine
    fade_out = int(min(max(0.3 * n, 0.05 * SR), 0.6 * SR))
    seg[n - fade_out:] *= 0.5 * (1 + np.cos(np.linspace(0, math.pi, fade_out)))
    seg *= 10 ** (PEAK_DBFS / 20) / np.abs(seg).max()
    return seg


# ------------------------------------------------------------------ analysis

def frame_f0(frame: np.ndarray, fmin: float = 60.0, fmax: float = 1500.0) -> tuple[float, float]:
    """Normalized-autocorrelation f0 with first-strong-peak picking (avoids octave-down errors)."""
    frame = frame - frame.mean()
    n = len(frame)
    spec = np.fft.rfft(frame * np.hanning(n), 2 * n)
    ac = np.fft.irfft(np.abs(spec) ** 2)[:n]
    if ac[0] <= 0:
        return 0.0, 0.0
    ac /= ac[0]
    lo, hi = int(SR / fmax), min(int(SR / fmin), n - 2)
    best = lo + int(np.argmax(ac[lo:hi]))
    k = best
    for j in range(lo + 1, hi - 1):
        if ac[j] > 0.85 * ac[best] and ac[j] >= ac[j - 1] and ac[j] >= ac[j + 1]:
            k = j
            break
    a, b, c = ac[k - 1], ac[k], ac[k + 1]
    shift = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
    return SR / (k + shift), float(ac[k])


def pitch_track(x: np.ndarray, kind: str) -> tuple[float, float, int]:
    """Median f0 over the steady part, its spread in cents, and frames used."""
    begin = 0.08 if kind == "pluck" else 0.25
    end = min(len(x) / SR * 0.7, 1.2 if kind == "pluck" else 2.5)
    win, hop = 4096, 1024
    f0s = []
    for s in range(int(begin * SR), int(end * SR) - win, hop):
        f, conf = frame_f0(x[s:s + win])
        if f > 0 and conf > 0.6:
            f0s.append(f)
    if not f0s:
        return 0.0, float("nan"), 0
    f0s = np.array(f0s)
    med = float(np.median(f0s))
    cents = 1200 * np.log2(f0s / med)
    return med, float(np.std(cents)), len(f0s)


def count_onsets(x: np.ndarray) -> int:
    win, hop = 2048, 512
    if len(x) < win * 2:
        return 1
    frames = np.lib.stride_tricks.sliding_window_view(x, win)[::hop]
    mag = np.abs(np.fft.rfft(frames * np.hanning(win), axis=1))
    flux = np.maximum(0, np.diff(np.log1p(mag * 10), axis=0)).sum(1)
    rms = np.sqrt((frames ** 2).mean(1))[1:]
    thr = np.median(flux) + 4 * np.std(flux)
    peaks = [i for i in range(1, len(flux) - 1)
             if flux[i] > thr and flux[i] >= flux[i - 1] and flux[i] >= flux[i + 1] and rms[i] > 0.05]
    merged: list[int] = []
    for p in peaks:
        if not merged or p - merged[-1] > int(0.15 * SR / hop):
            merged.append(p)
    return max(1, len(merged))


def midi_of(freq: float) -> tuple[int, float]:
    m = 69 + 12 * math.log2(freq / 440.0)
    return int(round(m)), (m - round(m)) * 100


# ---------------------------------------------------------------------- main

def credits_md(rows: list[dict]) -> str:
    lines = [
        "# Chinese instrument samples - credits",
        "",
        "Single-note / single-hit samples fetched and processed by `tools/fetch_chinese_samples.py`",
        "(44.1 kHz mono 16-bit, trimmed, tail-faded, peak-normalized to -1 dBFS).",
        "All sources are Freesound HQ previews. Root notes use LGPT naming (C3 = MIDI 60).",
        "",
        "CC BY files require attribution: credit the author and link the source page and license",
        "wherever these samples (or songs/projects that ship them) are distributed.",
        "",
        "| File | Instrument | Root (LGPT) | Author | License | Source |",
        "|------|------------|-------------|--------|---------|--------|",
    ]
    for r in rows:
        s: Source = r["src"]
        lines.append(
            f"| `{s.slug}.wav` | {s.instrument} | {r['root']} | {s.author} | "
            f"[{s.license[0]}]({s.license[1]}) | [\"{s.title}\"]({s.page_url}) |")
    lines += ["", "## Notes", ""]
    for r in rows:
        s = r["src"]
        extra = f" {s.notes}" if s.notes else ""
        lines.append(f"- `{s.slug}.wav`: \"{s.title}\" by {s.author} ({s.page_url}), "
                     f"{s.license[0]}. Cut from {s.start:.2f} s, {r['dur']:.2f} s kept.{extra}")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("slugs", nargs="*", help="only process these slugs")
    ap.add_argument("--refresh", action="store_true", help="re-download cached sources")
    args = ap.parse_args()

    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    wanted = [s for s in SOURCES if not args.slugs or s.slug in args.slugs]
    unknown = set(args.slugs) - {s.slug for s in SOURCES}
    if unknown:
        raise SystemExit(f"unknown slug(s): {', '.join(sorted(unknown))}")

    rows = []
    print(f"{'slug':10} {'dur':>5} {'peak':>6} {'rms':>6} {'on':>2}  {'root':6} {'Hz':>7} {'cents':>6} {'spread':>6}")
    for src in wanted:
        raw = decode(download(src, args.refresh))
        out = process(src, raw)
        path = OUT_DIR / f"{src.slug}.wav"
        write_wav(path, out)
        x = read_wav(path)  # verify what actually landed on disk
        dur = len(x) / SR
        peak_db = 20 * math.log10(np.abs(x).max())
        rms_db = 20 * math.log10(np.sqrt(np.mean(x ** 2)))
        clipped = int(np.sum(np.abs(x) >= 32767 / 32768))
        onsets = count_onsets(x)
        root, hz, cents, spread = "-", 0.0, 0.0, float("nan")
        if src.pitched:
            hz, spread, _ = pitch_track(x, src.kind)
            if hz <= 0:
                raise RuntimeError(f"{src.slug}: no stable pitch found")
            midi, cents = midi_of(hz)
            root = note_name(midi)
        if clipped:
            raise RuntimeError(f"{src.slug}: {clipped} clipped samples")
        if rms_db < -45:
            raise RuntimeError(f"{src.slug}: nearly silent ({rms_db:.1f} dBFS RMS)")
        print(f"{src.slug:10} {dur:5.2f} {peak_db:6.1f} {rms_db:6.1f} {onsets:2d}  {root:6} "
              f"{hz:7.1f} {cents:+6.0f} {spread:6.1f}")
        rows.append({"src": src, "dur": dur, "root": root if src.pitched else "unpitched"})

    if not args.slugs:
        (OUT_DIR / "CREDITS.md").write_text(credits_md(rows), encoding="utf-8")
        print(f"wrote {len(rows)} samples + CREDITS.md to {OUT_DIR.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
