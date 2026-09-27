#!/usr/bin/env python3
"""Golden audio: does every sound still sound the way it did?

Renders every SYNTH and MACRO preset, every shipped sample and the start of
every demo song on the device's own code (tools/dsp-harness/golden_audio.sh,
qemu-arm, deterministic), fingerprints each render and compares it with
tests/golden/audio.json.

    python tools/golden_audio.py                 # render + compare
    python tools/golden_audio.py --update        # render + accept as golden
    python tools/golden_audio.py --no-render     # compare the last render
    python tools/golden_audio.py --only fm4      # just ids containing fm4

A fingerprint is what you'd hear change: the level envelope (50 ms blocks,
250 ms for songs), peak, when the sound starts and ends, left/right
balance, energy per octave band, brightness (spectral centroid) and pitch.
A failure names the sound and says how it changed ("level -3.1 dB",
"pitch 261.6 -> 277.2 Hz (+100 cents)", "length 1450 -> 900 ms") and
writes build/golden-audio/diff/<id>-before.wav / -after.wav (before = the
render --update last accepted on this machine, when there is one).

--update rewrites the goldens deliberately and prints what changed, so the
commit that changes a sound shows it in tests/golden/audio.json.
"""

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
import wave

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden", "audio.json")
WORK = os.path.join(ROOT, "build", "golden-audio")
CURRENT = os.path.join(WORK, "current")
REFERENCE = os.path.join(WORK, "reference")
DIFF = os.path.join(WORK, "diff")
RATE = 44100
FLOOR = -90.0

# How far a fingerprint may move before it counts as a changed sound. The
# renders are deterministic (same code, same numbers), so these only have to
# absorb noise-based sounds drawing different random numbers.
TOL = {
    "level_db": 1.0,       # loudest block
    "env_db": 3.0,         # any block louder than -50 dB
    "peak_db": 1.5,
    "onset_ms": 20.0,
    "length_ms": 60.0,     # or 6 %, whichever is larger
    "length_rel": 0.06,
    "pan_db": 1.0,
    "band_db": 3.0,        # octave bands above -40 dB (relative)
    "centroid_rel": 0.08,
    "pitch_cents": 15.0,
}
BANDS = [63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]


def wsl_path(path):
    path = os.path.abspath(path)
    drive = path[0].lower()
    return "/mnt/%s%s" % (drive, path[2:].replace("\\", "/"))


def read_wav(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        ch = w.getnchannels()
        data = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768.0
    if ch == 1:
        return data, data
    data = data.reshape(-1, ch)
    return data[:, 0], data[:, 1]


def db(x):
    return FLOOR if x <= 1e-9 else max(FLOOR, 20.0 * math.log10(x))


def pitch_of(sig):
    """f0 of the loudest 200 ms by normalized autocorrelation, or None"""
    win = int(0.2 * RATE)
    if len(sig) < win:
        return None
    hop = win // 4
    best, start = -1.0, 0
    for s in range(0, len(sig) - win + 1, hop):
        e = float(np.sum(sig[s:s + win] ** 2))
        if e > best:
            best, start = e, s
    seg = sig[start:start + win] - np.mean(sig[start:start + win])
    if np.max(np.abs(seg)) < 1e-4:
        return None
    n = len(seg)
    spec = np.fft.rfft(seg, 2 * n)
    ac = np.fft.irfft(spec * np.conj(spec))[:n]
    if ac[0] <= 0:
        return None
    ac = ac / ac[0]
    lo, hi = int(RATE / 2000), int(RATE / 40)
    lag = lo + int(np.argmax(ac[lo:hi]))
    clarity = ac[lag]
    # Prefer the shortest lag that is nearly as strong (avoid octave errors)
    for cand in range(lo, lag):
        if ac[cand] > 0.9 * clarity and ac[cand] >= ac[cand - 1] and ac[cand] >= ac[cand + 1]:
            lag = cand
            clarity = ac[cand]
            break
    if clarity < 0.6:
        return None
    # Parabolic interpolation for a finer lag
    if 1 <= lag < n - 1:
        a, b, c = ac[lag - 1], ac[lag], ac[lag + 1]
        denom = a - 2 * b + c
        shift = 0.5 * (a - c) / denom if denom != 0 else 0.0
    else:
        shift = 0.0
    return round(RATE / (lag + shift), 2)


def fingerprint(path, kind):
    L, R = read_wav(path)
    mono = 0.5 * (L + R)
    song = kind == "demo"
    block = int((0.25 if song else 0.05) * RATE)
    env = []
    for s in range(0, len(mono), block):
        seg = mono[s:s + block]
        env.append(round(db(float(np.sqrt(np.mean(seg ** 2)))) if len(seg) else FLOOR, 1))
    loud = np.maximum(np.abs(L), np.abs(R))
    above = np.nonzero(loud > 0.001)[0]
    onset = 1000.0 * above[0] / RATE if len(above) else None
    end = 1000.0 * above[-1] / RATE if len(above) else None
    rl = float(np.sqrt(np.mean(L ** 2))) if len(L) else 0.0
    rr = float(np.sqrt(np.mean(R ** 2))) if len(R) else 0.0
    pan = round(db(rl) - db(rr), 2) if rl > 1e-6 and rr > 1e-6 else 0.0
    # Spectrum of the loudest half second (a song: all of it, in chunks)
    win = min(len(mono), int(0.5 * RATE))
    fp = {
        "kind": kind,
        "seconds": round(len(mono) / RATE, 3),
        "env": env,
        "level": max(env) if env else FLOOR,
        "peak": round(db(float(np.max(loud))) if len(loud) else FLOOR, 2),
        "onset_ms": None if onset is None else round(onset, 1),
        "end_ms": None if end is None else round(end, 1),
        "pan_db": pan,
        "bands": None,
        "centroid": None,
        "pitch": None,
    }
    if win < 1024 or not len(above):
        return fp
    if song:
        chunks = [mono[s:s + win] for s in range(0, len(mono) - win + 1, win)]
    else:
        hop = win // 4
        best, start = -1.0, 0
        for s in range(0, len(mono) - win + 1, hop):
            e = float(np.sum(mono[s:s + win] ** 2))
            if e > best:
                best, start = e, s
        chunks = [mono[start:start + win]]
    power = None
    for c in chunks:
        p = np.abs(np.fft.rfft(c * np.hanning(len(c)))) ** 2
        power = p if power is None else power + p
    freqs = np.fft.rfftfreq(win, 1.0 / RATE)
    total = float(np.sum(power)) or 1e-20
    bands = []
    for f in BANDS:
        sel = (freqs >= f / math.sqrt(2)) & (freqs < f * math.sqrt(2))
        bands.append(round(10.0 * math.log10(max(float(np.sum(power[sel])) / total, 1e-9)), 1))
    fp["bands"] = bands
    fp["centroid"] = round(float(np.sum(freqs * power) / total), 1)
    if not song:
        fp["pitch"] = pitch_of(mono)
    return fp


def compare(old, new):
    """What changed, in words; [] when it sounds the same"""
    out = []
    if old["level"] is not None and abs(new["level"] - old["level"]) > TOL["level_db"]:
        out.append("level %+.1f dB (%.1f -> %.1f)" % (new["level"] - old["level"], old["level"], new["level"]))
    if abs(new["peak"] - old["peak"]) > TOL["peak_db"]:
        out.append("peak %+.1f dB" % (new["peak"] - old["peak"]))
    if (old["end_ms"] is None) != (new["end_ms"] is None):
        out.append("silent" if new["end_ms"] is None else "no longer silent")
        return out
    if old["onset_ms"] is not None and abs(new["onset_ms"] - old["onset_ms"]) > TOL["onset_ms"]:
        out.append("starts %d -> %d ms" % (old["onset_ms"], new["onset_ms"]))
    if old["end_ms"] is not None:
        allowed = max(TOL["length_ms"], TOL["length_rel"] * old["end_ms"])
        if abs(new["end_ms"] - old["end_ms"]) > allowed:
            out.append("length %d -> %d ms" % (old["end_ms"], new["end_ms"]))
    worst, where = 0.0, 0
    block_ms = 250 if new["kind"] == "demo" else 50
    for i in range(min(len(old["env"]), len(new["env"]))):
        a, b = old["env"][i], new["env"][i]
        if max(a, b) > -50 and abs(b - a) > abs(worst):
            worst, where = b - a, i
    if abs(worst) > TOL["env_db"]:
        out.append("envelope %+.1f dB at %d ms" % (worst, where * block_ms))
    if abs(new["pan_db"] - old["pan_db"]) > TOL["pan_db"]:
        out.append("balance L-R %+.1f -> %+.1f dB" % (old["pan_db"], new["pan_db"]))
    if old["centroid"] and new["centroid"]:
        rel = new["centroid"] / old["centroid"] - 1.0
        if abs(rel) > TOL["centroid_rel"]:
            out.append("%s: centroid %d -> %d Hz (%+d%%)" % ("brighter" if rel > 0 else "darker",
                       old["centroid"], new["centroid"], round(100 * rel)))
    if old["bands"] and new["bands"]:
        moved = ["%s %+.1f" % (("%dHz" % f) if f < 1000 else ("%dk" % (f // 1000)), b - a)
                 for f, a, b in zip(BANDS, old["bands"], new["bands"])
                 if max(a, b) > -40 and abs(b - a) > TOL["band_db"]]
        if moved:
            out.append("bands dB: " + ", ".join(moved))
    if old.get("pitch") and new.get("pitch"):
        cents = 1200.0 * math.log2(new["pitch"] / old["pitch"])
        if abs(cents) > TOL["pitch_cents"]:
            out.append("pitch %.1f -> %.1f Hz (%+d cents)" % (old["pitch"], new["pitch"], round(cents)))
    elif bool(old.get("pitch")) != bool(new.get("pitch")) and new["kind"] != "demo":
        out.append("pitch %s -> %s" % (old.get("pitch") or "none", new.get("pitch") or "none"))
    return out


def render(jobs, only, demo_seconds, no_build):
    os.makedirs(CURRENT, exist_ok=True)
    env_prefix = []
    if only:
        env_prefix.append("GOLDEN_ONLY=%s" % only)
    if no_build:
        env_prefix.append("GOLDEN_NO_BUILD=1")
    script = wsl_path(os.path.join(ROOT, "tools", "dsp-harness", "golden_audio.sh"))
    cmd = "%s bash '%s' '%s' %d %s" % (" ".join(env_prefix), script, wsl_path(CURRENT), jobs, demo_seconds)
    started = time.time()
    # CREATE_NO_WINDOW: no console window pops up and steals focus
    flags = 0x08000000 if os.name == "nt" else 0
    proc = subprocess.run(["wsl", "-e", "bash", "-lc", cmd], creationflags=flags,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(proc.stdout.strip())
    print("rendered in %.0f s" % (time.time() - started))
    return proc.returncode


def load_index():
    index = {}
    for name in os.listdir(CURRENT):
        if name.startswith("index-") and name.endswith(".txt"):
            with open(os.path.join(CURRENT, name)) as f:
                for line in f:
                    parts = line.rstrip("\n").split("\t")
                    if len(parts) == 2:
                        index[parts[0]] = parts[1].split(" ")[0]
    return index


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--update", action="store_true", help="accept this render as the goldens")
    ap.add_argument("--no-render", action="store_true", help="compare the last render")
    ap.add_argument("--no-build", action="store_true", help="don't rebuild the device objects")
    ap.add_argument("--only", default="", help="only ids containing this text")
    ap.add_argument("--jobs", type=int, default=max(2, min(8, (os.cpu_count() or 4) - 2)))
    ap.add_argument("--demo-seconds", type=float, default=10.0)
    args = ap.parse_args(argv)

    if not args.no_render:
        if render(args.jobs, args.only, args.demo_seconds, args.no_build) != 0:
            print("RENDER FAILED (see build/golden-audio/current/*.log)")
            return 2
    index = load_index()
    current = {}
    for sid in sorted(index):
        path = os.path.join(CURRENT, sid + ".wav")
        if os.path.exists(path):
            current[sid] = fingerprint(path, index[sid])
    golden = {"sounds": {}}
    if os.path.exists(GOLDEN):
        with open(GOLDEN) as f:
            golden = json.load(f)
    old = golden["sounds"]
    scope = [sid for sid in old if not args.only or args.only in sid]

    changes = {}
    for sid, fp in current.items():
        if sid not in old:
            changes[sid] = ["new sound (not in the goldens)"]
        else:
            diff = compare(old[sid], fp)
            if diff:
                changes[sid] = diff
    for sid in scope:
        if sid not in current:
            changes[sid] = ["missing: no render (removed or failed)"]

    for sid in sorted(changes):
        print("%-40s %s" % (sid, "; ".join(changes[sid])))
    kinds = {}
    for fp in current.values():
        kinds[fp["kind"]] = kinds.get(fp["kind"], 0) + 1
    print("%d sounds (%s), %d changed" % (len(current),
          ", ".join("%d %s" % (n, k) for k, n in sorted(kinds.items())), len(changes)))

    if args.update:
        if args.only:
            merged = dict(old)
            for sid in scope:
                merged.pop(sid, None)
            merged.update(current)
        else:
            merged = current
        golden = {
            "about": "Fingerprints of every preset, shipped sample and demo song start, "
                     "rendered on the device code. Regenerate: python tools/golden_audio.py --update",
            "sounds": {k: merged[k] for k in sorted(merged)},
        }
        os.makedirs(os.path.dirname(GOLDEN), exist_ok=True)
        with open(GOLDEN, "w", newline="\n") as f:
            json.dump(golden, f, indent=0, separators=(",", ":"))
            f.write("\n")
        os.makedirs(REFERENCE, exist_ok=True)
        for sid in current:
            shutil.copyfile(os.path.join(CURRENT, sid + ".wav"), os.path.join(REFERENCE, sid + ".wav"))
        print("goldens updated: %s (%d sounds)" % (os.path.relpath(GOLDEN, ROOT), len(golden["sounds"])))
        return 0

    if changes:
        os.makedirs(DIFF, exist_ok=True)
        for sid in changes:
            after = os.path.join(CURRENT, sid + ".wav")
            before = os.path.join(REFERENCE, sid + ".wav")
            if os.path.exists(after):
                shutil.copyfile(after, os.path.join(DIFF, sid + "-after.wav"))
            if os.path.exists(before):
                shutil.copyfile(before, os.path.join(DIFF, sid + "-before.wav"))
        print("GOLDEN AUDIO FAILED: %d sound(s) changed. Listen: %s" % (len(changes), os.path.relpath(DIFF, ROOT)))
        print("If the change is intended: python tools/golden_audio.py --update")
        return 1
    print("GOLDEN AUDIO OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
