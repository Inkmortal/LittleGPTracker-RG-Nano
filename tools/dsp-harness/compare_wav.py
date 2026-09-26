"""Compare two renders of the same song (e.g. engine_room_check.cpp's WAV
before and after a DSP change).

    python tools/dsp-harness/compare_wav.py reference.wav test.wav [--from S] [--to S]

Reports, relative to full scale (dBFS):
  - the level of the difference signal (RMS and peak), per channel
  - where the difference is largest (1 s windows)
  - both spectra (Welch, Hann) per octave band and their difference, so an
    intentional algorithm change can be judged: same tone, and no more
    energy where only aliasing would put it
"""

import argparse
import sys
import wave

import numpy as np


def load(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            sys.exit(f"{path}: 16-bit WAV expected")
        rate = w.getframerate()
        ch = w.getnchannels()
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    return rate, data.reshape(-1, ch) / 32768.0


def db(x):
    return 20.0 * np.log10(max(float(x), 1e-12))


def spectrum(x, rate, n=8192):
    """Welch power spectrum (Hann, 50% overlap) of a mono signal."""
    win = np.hanning(n)
    hop = n // 2
    frames = [x[i:i + n] * win for i in range(0, len(x) - n, hop)]
    if not frames:
        return np.fft.rfftfreq(n, 1.0 / rate), np.zeros(n // 2 + 1)
    p = np.mean([np.abs(np.fft.rfft(f)) ** 2 for f in frames], axis=0)
    p /= (np.sum(win) ** 2) / 4.0   # a full-scale sine reads 0 dB
    return np.fft.rfftfreq(n, 1.0 / rate), p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference")
    ap.add_argument("test")
    ap.add_argument("--from", dest="start", type=float, default=0.0)
    ap.add_argument("--to", dest="end", type=float, default=None)
    args = ap.parse_args()

    ra, a = load(args.reference)
    rb, b = load(args.test)
    if ra != rb or a.shape[1] != b.shape[1]:
        sys.exit("different rate or channel count")
    n = min(len(a), len(b))
    s = int(args.start * ra)
    e = n if args.end is None else min(n, int(args.end * ra))
    a, b = a[s:e], b[s:e]
    print(f"compared {len(a) / ra:.2f} s ({args.reference} vs {args.test})")

    d = b - a
    for c in range(a.shape[1]):
        name = "LR"[c] if a.shape[1] == 2 else str(c)
        print(f"  {name}: signal RMS {db(np.sqrt(np.mean(a[:, c] ** 2))):7.1f} dBFS, "
              f"difference RMS {db(np.sqrt(np.mean(d[:, c] ** 2))):7.1f} dBFS, "
              f"peak {db(np.max(np.abs(d[:, c]))):7.1f} dBFS")
    rms = np.sqrt(np.mean(d ** 2))
    print(f"difference overall: RMS {db(rms):.1f} dBFS, peak {db(np.max(np.abs(d))):.1f} dBFS "
          f"(signal-to-difference {db(np.sqrt(np.mean(a ** 2))) - db(rms):.1f} dB)")

    # worst seconds
    sec = ra
    worst = []
    for i in range(0, len(d) - sec + 1, sec):
        worst.append((np.max(np.abs(d[i:i + sec])), i / ra))
    worst.sort(reverse=True)
    print("largest differences (1 s windows): " +
          ", ".join(f"{t + args.start:.0f}s {db(v):.1f}" for v, t in worst[:5]))

    # spectra per octave band (mono sum)
    fa, pa = spectrum(a.mean(axis=1), ra)
    _, pb = spectrum(b.mean(axis=1), ra)
    _, pd = spectrum(d.mean(axis=1), ra)
    edges = [20, 40, 80, 160, 315, 630, 1250, 2500, 5000, 10000, 16000, 20000, ra / 2]
    print(f"{'band Hz':>13} {'ref dB':>8} {'test dB':>8} {'test-ref':>9} {'diff dB':>8}")
    for lo, hi in zip(edges[:-1], edges[1:]):
        m = (fa >= lo) & (fa < hi)
        if not np.any(m):
            continue
        ea, eb, ed = (10 * np.log10(max(np.sum(p[m]), 1e-24)) for p in (pa, pb, pd))
        print(f"{lo:6.0f}-{hi:6.0f} {ea:8.1f} {eb:8.1f} {eb - ea:9.2f} {ed:8.1f}")


if __name__ == "__main__":
    main()
