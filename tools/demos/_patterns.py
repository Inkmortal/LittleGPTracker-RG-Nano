"""Reusable tracker idioms for demo songs (one phrase = one bar of 16 steps)."""

from __future__ import annotations

from typing import Sequence

from lgpt_composer import NO_INSTR, Phrase, chrd_param, note, voice_chord


def rest() -> Phrase:
    """A silent bar that also stops whatever the track was still holding."""
    return Phrase().set(0, cmd1="KILL", param1=0)


def silent() -> Phrase:
    """A bar with nothing in it: the previous note keeps ringing."""
    return Phrase()


def chord_bar(voicing: Sequence[int], instr: int | None, extra: tuple[str, int] | None = None) -> Phrase:
    """Whole-bar chord: lowest note + CHRD for the notes above it."""
    p = Phrase().set(0, note=voicing[0], instr=NO_INSTR if instr is None else instr)
    p.command(0, "CHRD", chrd_param(voicing))
    if extra:
        p.command(0, extra[0], extra[1])
    return p


def voice_progression(chords: Sequence[str], center: str, span: int = 15) -> list[list[int]]:
    """Voice a progression with smooth voice leading around `center`."""
    out: list[list[int]] = []
    previous = None
    for symbol in chords:
        v = voice_chord(symbol, note(center), previous, span)
        out.append(v)
        previous = v
    return out


ARP_UPDOWN = [0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 3]
ARP_UP = [0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3]


def arp_bar(voicing: Sequence[int], instr: int | None, pattern: Sequence[int] = ARP_UPDOWN,
            first_instr: int | None = None, step_every: int = 1) -> Phrase:
    """16th-note arpeggio over chord tones; index len(voicing) = root + octave."""
    tones = list(voicing[:3]) + [voicing[0] + 12]
    p = Phrase()
    for i in range(0, 16, step_every):
        idx = pattern[i % len(pattern)] % len(tones)
        use = instr
        if i == 0 and first_instr is not None:
            use = first_instr
        p.set(i, note=tones[idx], instr=NO_INSTR if use is None else use)
    return p


def octave_bass(root: str, instr: int, approach: str | None = None, volume_off: int | None = None) -> Phrase:
    """Synthwave 8th-note octave bass on `root` (written pitch), optional walk-up note."""
    low = note(root)
    p = Phrase()
    for i in range(0, 16, 2):
        n = low if (i // 2) % 2 == 0 else low + 12
        p.set(i, note=n, instr=instr)
        if volume_off is not None and (i // 2) % 2 == 1:
            p.command(i, "VOLM", volume_off)
    if approach:
        p.set(14, note=note(approach), instr=instr)
    return p


def crescendo(pattern: str, instr: int, start: int, end: int, pitch: int = 60) -> Phrase:
    """Drum hits whose volume ramps from start to end across the bar."""
    p = Phrase()
    hits = [i for i, ch in enumerate(pattern.replace(" ", "")) if ch != "."]
    for k, i in enumerate(hits):
        vol = start + (end - start) * k // max(1, len(hits) - 1)
        p.set(i, note=pitch, instr=instr)
        p.command(i, "VOLM", vol)
    return p
