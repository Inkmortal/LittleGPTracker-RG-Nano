#!/usr/bin/env python3
"""Synthesize the drum kits, textures, guitar and bass samples used by the
genre demos. Everything here is generated from scratch (no third-party audio),
so the output is original and free to use.

Packs written to projects/resources/samples/<pack>/:
  drums-808    kick (long boom), kick-short, snare, clap, hat, openhat, cowbell, rim
  drums-909    kick, snare, clap, hat, openhat, ride, crash
  drums-dusty  kick, snare, hat, rim  (boom-bap / lo-fi: saturated, dark, crunchy)
  drums-jazz   ride, ride-bell, brush-swish, brush-tap, kick
  textures     vinyl (crackle bed), tape-hiss, riser
  guitar       power-e (E power chord, overdriven), mute (palm-muted chug),
               clean (clean single note)
  bass         finger (electric bass, finger style), reese (detuned saw bass)
  epiano       note (an FM "tine" electric piano like a Rhodes) and chord
               one-shots: maj7, min7, dom7, min9, maj9, maj, min
  chords       piano chord one-shots mixed from the VSCO upright piano
               (needs tools/fetch_vsco_samples.py first) and string-section
               chord pads (maj, min) from the VSCO violins and cellos

Chord one-shots are how sample-based lo-fi and hip hop play harmony: one
track plays the whole chord and the note transposes it. All chords are
rooted on A2 (LGPT naming).

Each pack gets samples.json (root note per sample, LGPT naming C3 = MIDI 60)
and a CREDITS.md. Usage:  python tools/make_samples.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from scipy import signal

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from fetch_chinese_samples import read_wav, write_wav  # noqa: E402

SR = 44100
SAMPLES = ROOT / "projects" / "resources" / "samples"
RNG = np.random.default_rng(7)


# ------------------------------------------------------------------ helpers

def t(seconds: float) -> np.ndarray:
    return np.arange(int(seconds * SR)) / SR


def env(seconds: float, decay: float, attack: float = 0.001) -> np.ndarray:
    x = t(seconds)
    a = np.clip(x / attack, 0, 1) if attack > 0 else 1.0
    return a * np.exp(-x / decay)


def noise(seconds: float) -> np.ndarray:
    return RNG.uniform(-1, 1, int(seconds * SR))


def filt(x: np.ndarray, kind: str, freq, order: int = 2) -> np.ndarray:
    sos = signal.butter(order, freq, btype=kind, fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def sweep_sine(seconds: float, f_start: float, f_end: float, tau: float) -> np.ndarray:
    """Sine whose pitch falls exponentially from f_start to f_end."""
    x = t(seconds)
    f = f_end + (f_start - f_end) * np.exp(-x / tau)
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def metallic(seconds: float, freqs) -> np.ndarray:
    """Sum of square waves at inharmonic ratios (the 808/909 cymbal trick)."""
    x = t(seconds)
    return sum(signal.square(2 * np.pi * f * x) for f in freqs) / len(freqs)


def drive(x: np.ndarray, amount: float) -> np.ndarray:
    return np.tanh(x * amount) / np.tanh(amount)


def crush(x: np.ndarray, bits: int, rate: int) -> np.ndarray:
    """Bit depth and sample-rate reduction, for dusty sampler character."""
    step = SR / rate
    held = x[(np.floor(np.arange(len(x)) / step) * step).astype(int)]
    q = 2 ** (bits - 1)
    return np.round(held * q) / q


def finish(x: np.ndarray, fade: float = 0.01, peak_db: float = -1.0) -> np.ndarray:
    x = x - np.mean(x)
    n = int(fade * SR)
    if n and len(x) > n:
        x[-n:] *= np.linspace(1, 0, n)
    return x * (10 ** (peak_db / 20) / (np.abs(x).max() + 1e-12))


def karplus(freq: float, seconds: float, damping: float = 0.996, brightness: float = 0.5) -> np.ndarray:
    """Karplus-Strong plucked string."""
    n = int(seconds * SR)
    period = int(round(SR / freq))
    buf = filt(RNG.uniform(-1, 1, period), "lowpass", 400 + brightness * 8000, 1)
    out = np.zeros(n)
    for i in range(n):
        j = i % period
        out[i] = buf[j]
        buf[j] = damping * 0.5 * (buf[j] + buf[(j + 1) % period])
    return out


# ------------------------------------------------------------------ kits

def kit_808() -> dict:
    s = {}
    s["kick"] = finish(drive(sweep_sine(1.4, 160, 44, 0.05) * env(1.4, 0.55, 0.002), 1.6), 0.08)
    s["kick-short"] = finish(drive(sweep_sine(0.45, 150, 50, 0.04) * env(0.45, 0.14), 2.0))
    tone = (np.sin(2 * np.pi * 180 * t(0.3)) + 0.6 * np.sin(2 * np.pi * 330 * t(0.3))) * env(0.3, 0.06)
    s["snare"] = finish(tone + filt(noise(0.3), "highpass", 1800) * env(0.3, 0.09) * 0.9)
    bursts = sum(np.roll(env(0.35, 0.008), int(k * 0.011 * SR)) * (k < 3) for k in range(3))
    tail = env(0.35, 0.12)
    s["clap"] = finish(filt(noise(0.35), "bandpass", [900, 2200]) * (bursts + tail * 0.8))
    cym = filt(metallic(0.6, [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]), "highpass", 7000, 4)
    s["hat"] = finish(cym[: int(0.12 * SR)] * env(0.12, 0.025))
    s["openhat"] = finish(cym * env(0.6, 0.18))
    bell = filt(signal.square(2 * np.pi * 540 * t(0.4)) + signal.square(2 * np.pi * 800 * t(0.4)),
                "bandpass", [500, 3000])
    s["cowbell"] = finish(bell * env(0.4, 0.09))
    s["rim"] = finish(filt(noise(0.06) + np.sin(2 * np.pi * 1700 * t(0.06)), "bandpass", [1200, 2600]) * env(0.06, 0.01))
    return s


def kit_909() -> dict:
    s = {}
    click = filt(noise(0.01), "highpass", 3000) * env(0.01, 0.002)
    body = sweep_sine(0.6, 230, 52, 0.025) * env(0.6, 0.18, 0.001)
    s["kick"] = finish(drive(body + np.pad(click, (0, len(body) - len(click))) * 0.5, 2.2))
    tone = np.sin(2 * np.pi * 190 * t(0.35)) * env(0.35, 0.05)
    s["snare"] = finish(tone * 0.8 + filt(noise(0.35), "lowpass", 9000) * env(0.35, 0.12))
    bursts = sum(np.roll(env(0.4, 0.007), int(k * 0.009 * SR)) * (k < 4) for k in range(4))
    s["clap"] = finish(filt(noise(0.4), "bandpass", [1000, 2600]) * (bursts + env(0.4, 0.15) * 0.7))
    hat = filt(noise(0.7) * 0.6 + metallic(0.7, [263, 400, 421, 474, 587, 845]) * 0.4, "highpass", 8000, 4)
    s["hat"] = finish(hat[: int(0.1 * SR)] * env(0.1, 0.02))
    s["openhat"] = finish(hat * env(0.7, 0.22))
    ride = filt(metallic(2.0, [410, 569, 752, 1033, 1411, 1826]) * 0.7 + noise(2.0) * 0.3, "highpass", 3500, 2)
    s["ride"] = finish(ride * env(2.0, 0.7), 0.1)
    crash = filt(noise(2.5) * 0.7 + metallic(2.5, [330, 457, 604, 811, 1107, 1502]) * 0.3, "highpass", 2500, 2)
    s["crash"] = finish(crash * env(2.5, 0.8, 0.002), 0.2)
    return s


def kit_dusty() -> dict:
    """Boom-bap / lo-fi: rounder, darker, saturated, 12-bit 26 kHz."""
    s = {}
    kick = sweep_sine(0.5, 120, 48, 0.035) * env(0.5, 0.16) + filt(noise(0.5), "lowpass", 800) * env(0.5, 0.01) * 0.3
    s["kick"] = finish(crush(filt(drive(kick, 3.0), "lowpass", 4000), 12, 26000))
    tone = np.sin(2 * np.pi * 200 * t(0.3)) * env(0.3, 0.04)
    snap = filt(noise(0.3), "bandpass", [1500, 6500]) * env(0.3, 0.07)
    s["snare"] = finish(crush(filt(drive(tone + snap * 1.1, 2.5), "lowpass", 7000), 12, 26000))
    hat = filt(noise(0.09), "bandpass", [5000, 9500]) * env(0.09, 0.02)
    s["hat"] = finish(crush(hat, 12, 26000))
    s["rim"] = finish(crush(filt(noise(0.05) + np.sin(2 * np.pi * 1500 * t(0.05)), "bandpass", [900, 2400])
                            * env(0.05, 0.012), 12, 26000))
    return s


def kit_jazz() -> dict:
    s = {}
    # A ride is mostly shimmer: bright noise that washes on, with a faint
    # high ping from many close inharmonic partials (low pure tones sound
    # like a cowbell)
    partials = [3120, 3480, 3910, 4270, 4790, 5230, 5860, 6410, 7020]
    ping = sum(np.sin(2 * np.pi * f * t(1.8) + RNG.uniform(0, 6.28)) for f in partials) / len(partials)
    wash = filt(noise(1.8), "bandpass", [3500, 13000])
    stick = filt(noise(0.02), "highpass", 6000) * env(0.02, 0.003)
    body = wash * 0.8 * env(1.8, 0.55) + ping * 0.25 * env(1.8, 0.25)
    body[: len(stick)] += stick * 1.5
    s["ride"] = finish(body, 0.3)
    bell = (sum(np.sin(2 * np.pi * f * 1.2 * t(1.2)) for f in partials[:4]) / 4 * env(1.2, 0.35)
            + filt(noise(1.2), "bandpass", [4000, 12000]) * 0.4 * env(1.2, 0.3))
    s["ride-bell"] = finish(bell, 0.2)
    swish = filt(noise(0.45), "bandpass", [2500, 9000]) * np.sin(np.pi * np.clip(t(0.45) / 0.45, 0, 1)) ** 1.5
    s["brush-swish"] = finish(swish, 0.05)
    s["brush-tap"] = finish(filt(noise(0.12), "bandpass", [1500, 8000]) * env(0.12, 0.03, 0.004))
    s["kick"] = finish(sweep_sine(0.4, 90, 55, 0.03) * env(0.4, 0.12))
    return s


def textures() -> dict:
    s = {}
    n = int(4.0 * SR)
    hiss = filt(noise(4.0), "bandpass", [300, 5000]) * 0.08
    clicks = np.zeros(n)
    for pos in RNG.integers(0, n - 200, 90):
        amp = RNG.uniform(0.2, 1.0) * (1 if RNG.random() < 0.8 else 2.5)
        clicks[pos:pos + 40] += amp * np.exp(-np.arange(40) / 6) * RNG.choice([-1, 1])
    crackle = filt(clicks, "bandpass", [800, 7000]) * 0.8
    rumble = filt(noise(4.0), "lowpass", 80) * 0.15
    s["vinyl"] = finish(hiss + crackle + rumble, 0.05, -6.0)
    s["tape-hiss"] = finish(filt(noise(3.0), "bandpass", [2000, 12000]), 0.05, -12.0)
    x = t(3.0)
    rise = filt(noise(3.0), "bandpass", [800, 9000]) * (x / 3.0) ** 2
    rise += np.sin(2 * np.pi * np.cumsum(200 + 1800 * (x / 3.0) ** 2) / SR) * (x / 3.0) ** 2 * 0.3
    s["riser"] = finish(rise, 0.02)
    return s


def guitar() -> dict:
    """Plucked-string model through an amp: overdrive plus a cabinet-style EQ."""
    def amp(x: np.ndarray, gain: float) -> np.ndarray:
        x = drive(filt(x, "highpass", 90), gain)
        x = filt(x, "lowpass", 4200, 4)
        return filt(x, "bandpass", [100, 6000])

    s = {}
    e2 = 82.41  # guitar low E, LGPT E1
    chord = karplus(e2, 2.2, 0.997, 0.8) + karplus(e2 * 1.5, 2.2, 0.997, 0.8) + 0.7 * karplus(e2 * 2, 2.2, 0.997, 0.8)
    s["power-e"] = finish(amp(chord, 7.0), 0.3)
    mute = karplus(e2, 0.35, 0.94, 0.5) + karplus(e2 * 1.5, 0.35, 0.94, 0.5)
    s["mute"] = finish(amp(mute, 6.0) * env(0.35, 0.12), 0.05)
    s["clean"] = finish(filt(karplus(220.0, 2.0, 0.998, 0.6), "lowpass", 5000), 0.3)
    return s


def bass() -> dict:
    s = {}
    e1 = 41.2  # bass low E, LGPT E0
    finger = karplus(e1 * 2, 2.0, 0.998, 0.25)  # E1 (LGPT) sounds an octave above the low string
    s["finger"] = finish(drive(filt(finger, "lowpass", 1800), 1.5), 0.3)
    x = t(2.0)
    f = 55.0
    saw = sum(signal.sawtooth(2 * np.pi * f * d * x) for d in (1.0, 1.007, 0.993)) / 3
    s["reese"] = finish(filt(drive(saw, 1.8), "lowpass", 900, 2) * env(2.0, 3.0, 0.005), 0.3)
    return s


CHORDS = {
    "maj": (0, 4, 7, 12),
    "min": (0, 3, 7, 12),
    "maj7": (0, 4, 7, 11),
    "min7": (0, 3, 7, 10),
    "dom7": (0, 4, 7, 10),
    "min9": (0, 3, 7, 10, 14),
    "maj9": (0, 4, 7, 11, 14),
}
A2 = 110.0 * 2  # LGPT A2 = MIDI 57 = 220 Hz


def shift(x: np.ndarray, semitones: float) -> np.ndarray:
    """Resample to change pitch (a sampler's way: shorter when higher)."""
    ratio = 2 ** (semitones / 12)
    return signal.resample(x, max(1, int(len(x) / ratio)))


def mix(parts: list[np.ndarray], length: float) -> np.ndarray:
    n = int(length * SR)
    out = np.zeros(n)
    for x in parts:
        m = min(n, len(x))
        out[:m] += x[:m]
    return out


def tine(freq: float, seconds: float) -> np.ndarray:
    """Electric piano: FM with a decaying index for the bark, a quick bell
    partial for the tine ping, soft saturation for warmth."""
    x = t(seconds)
    index = 0.4 + 2.2 * np.exp(-x / 0.35)
    body = np.sin(2 * np.pi * freq * x + index * np.sin(2 * np.pi * freq * x))
    ping = 0.18 * np.sin(2 * np.pi * freq * 14.1 * x) * np.exp(-x / 0.04)
    return drive((body + ping) * np.exp(-x / 1.6) * np.clip(x / 0.002, 0, 1), 1.3)


def epiano() -> dict:
    s = {"note": finish(tine(A2, 3.0), 0.3)}
    for name, iv in CHORDS.items():
        s[name] = finish(mix([tine(A2 * 2 ** (i / 12), 3.0) for i in iv], 3.0), 0.3)
    return s


def chords() -> dict:
    keys = SAMPLES / "keys"
    strings = SAMPLES / "strings"
    if not (keys / "piano-mid.wav").exists():
        raise SystemExit("run tools/fetch_vsco_samples.py first (the piano chords are built from it)")
    s = {}
    piano = read_wav(keys / "piano-mid.wav")      # A2
    soft = read_wav(keys / "piano-soft.wav")      # A2, softer
    for name, iv in CHORDS.items():
        s[f"piano-{name}"] = finish(mix([shift(piano, i) for i in iv], 3.0), 0.3)
        s[f"soft-{name}"] = finish(mix([shift(soft, i) for i in iv], 3.0), 0.3)
    violins = read_wav(strings / "violins-low.wav")  # A3
    cellos = read_wav(strings / "cellos.wav")        # C3
    for name, third in (("maj", 4), ("min", 3)):
        parts = [shift(cellos, -3), shift(cellos, 4), shift(violins, 0),
                 shift(violins, third), shift(violins, 7)]
        s[f"strings-{name}"] = finish(mix(parts, 3.0), 0.4)
    return s


PACKS = {
    "drums-808": (kit_808, {}),
    "drums-909": (kit_909, {}),
    "drums-dusty": (kit_dusty, {}),
    "drums-jazz": (kit_jazz, {}),
    "textures": (textures, {}),
    "guitar": (guitar, {"power-e": "E1", "mute": "E1", "clean": "A2"}),
    "bass": (bass, {"finger": "E1", "reese": "A0"}),
    "epiano": (epiano, {k: "A2" for k in ["note"] + list(CHORDS)}),
    "chords": (chords, {**{f"piano-{k}": "A2" for k in CHORDS}, **{f"soft-{k}": "A2" for k in CHORDS},
                        "strings-maj": "A2", "strings-min": "A2"}),
}


def main() -> int:
    only = set(sys.argv[1:])  # pack names to rebuild; none = all
    for pack, (make, roots) in PACKS.items():
        if only and pack not in only:
            continue
        folder = SAMPLES / pack
        folder.mkdir(parents=True, exist_ok=True)
        sounds = make()
        manifest_path = folder / "samples.json"
        manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
        for name, x in sounds.items():
            write_wav(folder / f"{name}.wav", np.clip(x, -1, 1))
            manifest[name] = {"root": roots.get(name, "C3"), "instrument": f"{pack} {name}",
                              "pitched": name in roots}
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        (folder / "CREDITS.md").write_text(
            f"# {pack} samples - credits\n\nSynthesized from scratch by `tools/make_samples.py` "
            "(no third-party audio). Free to use for anything; no attribution needed.\n",
            encoding="utf-8")
        print(f"{pack:12} {', '.join(sounds)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
