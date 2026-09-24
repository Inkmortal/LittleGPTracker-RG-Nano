"""Sample-pack helpers for the demo songs (projects/resources/samples)."""

from __future__ import annotations

import json
from pathlib import Path

from lgpt_composer import NO_INSTR, Phrase, Project, note, parse_chord

SAMPLES = Path(__file__).resolve().parents[2] / "projects" / "resources" / "samples"
_manifests: dict[str, dict] = {}


def info(ref: str) -> dict:
    pack, slug = ref.split("/")
    if pack not in _manifests:
        _manifests[pack] = json.loads((SAMPLES / pack / "samples.json").read_text())
    return _manifests[pack][slug]


def use(p: Project, slot: int, ref: str, volume: int = 0x80, root: str | None = None,
        **params: object) -> int:
    """Sample instrument from a pack, e.g. use(p, 3, "bass/upright-low").
    The root note comes from the pack's samples.json (or `root`, e.g. to play
    an 808 kick as a tuned bass); the file is copied into the song as
    <pack>-<slug>.wav so packs never collide."""
    pack, slug = ref.split("/")
    return p.sample(slot, SAMPLES / pack / f"{slug}.wav", root or info(ref)["root"], volume,
                    filename=f"{pack}-{slug}.wav", **params)


def chord_root(symbol: str, low: str = "E2") -> int:
    """Note that plays a chord one-shot (rooted on A2) as `symbol`'s root:
    within an octave from `low`, so the sample is pitched at most a few
    semitones away from where it was recorded."""
    pc, _ = parse_chord(symbol)
    start = note(low)
    return start + (pc - start) % 12


def chords(slots: dict[str, int], hits: list[tuple[int, str, str]] | list[tuple[int, str, str, int]],
           low: str = "E2") -> Phrase:
    """One bar of chord one-shots. slots maps a quality ('min7') to the
    instrument playing it; hits are (step, root, quality[, volume])."""
    p = Phrase()
    for hit in hits:
        step, root, quality = hit[0], hit[1], hit[2]
        p.set(step, note=chord_root(root, low), instr=slots[quality])
        if len(hit) > 3:
            p.command(step, "VOLM", hit[3])  # type: ignore[misc]
    return p


def notes(text: str, pick: dict[str, int] | int | None, split: str | None = None) -> Phrase:
    """Melody where each note uses the nearer of two recordings: pick is
    {'low': slot, 'high': slot} and notes at or above `split` use 'high'."""
    if isinstance(pick, int) or pick is None:
        return Phrase.parse(text, pick)
    p = Phrase.parse(text, None)
    for i, s in enumerate(p.steps):
        if s.note != 255:
            p.set(i, instr=pick["high"] if split and s.note >= note(split) else pick["low"])
    return p


def loop_bar(slot: int, pitch: str = "C3") -> Phrase:
    """A texture (vinyl, hiss) started once and left looping."""
    return Phrase.parse(f"{pitch} . . . . . . . . . . . . . . .", slot)


__all__ = ["NO_INSTR", "SAMPLES", "chord_root", "chords", "info", "loop_bar", "notes", "use"]
