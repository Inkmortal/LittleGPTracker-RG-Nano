#!/usr/bin/env python3
"""Measure each segment of a preset tour capture (one instrument per segment)."""
import sys, math, wave
import numpy as np
sys.path.insert(0, __import__("os").path.dirname(__file__))
from audio_report import read_wav, db, band_energy, spectral_centroid

path = sys.argv[1]
seg_seconds = float(sys.argv[2])
names = sys.argv[3].split(",")
data, rate = read_wav(path)
mono = data.mean(axis=1)
# find first onset
thr = 0.01
start = int(np.argmax(np.abs(mono) > thr))
print(f"{'name':10} {'peak':>7} {'rms':>7} {'cent':>7}  bands(sub,bass,lowmid,mid,pres,air)")
for i, name in enumerate(names):
    a = start + int(i * seg_seconds * rate)
    b = a + int(seg_seconds * rate)
    seg = data[a:b]
    if len(seg) < 100:
        break
    m = seg.mean(axis=1)
    peak = db(float(np.abs(seg).max()))
    # rms over the loudest 300ms window
    w = int(0.3 * rate)
    best = 0
    for s in range(0, max(1, len(m) - w), w // 4):
        best = max(best, float(np.sqrt(np.mean(m[s:s + w] ** 2))))
    be = band_energy(m, rate)
    print(f"{name:10} {peak:7.1f} {db(best):7.1f} {spectral_centroid(m, rate):7.0f}  " + " ".join(f"{v:6.1f}" for v in be.values()))
