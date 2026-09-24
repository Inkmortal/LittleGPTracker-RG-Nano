"""GARAGE ANTHEM - rock in E, 128 BPM.

Teaching points:
- One guitar track, two sounds: palm-muted chugs in the verse, open power
  chords in the chorus - the instrument column switches them per step.
- A power chord sample (root + fifth + octave) moves like any note: E, C,
  G and D power chords are the same sample at different pitches.
- The bass doubles the guitar's roots in steady eighths: that is the engine.
"""

from lgpt_composer import Phrase, Project
from _kit import use
from _patterns import rest

KICK, SNARE, HAT, CRASH, BASS, MUTE, POWER, LEAD, RIDE = range(9)
ROOTS = ["E1", "C1", "G1", "D1"]


def chug_bar(root: str) -> Phrase:
    p = Phrase()
    for i in range(0, 16, 2):
        p.set(i, note=_n(root), instr=MUTE)
        if i % 4:
            p.command(i, "VOLM", 0x90)
    return p


def power_bar(root: str) -> Phrase:
    p = Phrase.parse(f"{root} . . . . . {root} . . . {root} . . . . .", POWER)
    return p


def bass_bar(root: str) -> Phrase:
    p = Phrase()
    for i in range(0, 16, 2):
        p.set(i, note=_n(root), instr=BASS)
    return p


def _n(name: str) -> int:
    from lgpt_composer import note
    return note(name)


def build() -> Project:
    p = Project("GarageAnthem", tempo=128)
    p.key("E", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.set("pregain", 62)
    p.params["reverb size"] = "110"
    p.params["reverb damp"] = "140"

    use(p, KICK, "drums-dusty/kick", 0xC8)
    use(p, SNARE, "percussion/snare", 0xD0, reverb=0x40)
    use(p, HAT, "drums-909/hat", 0x70)
    use(p, CRASH, "drums-909/crash", 0x78)
    use(p, RIDE, "drums-909/ride", 0x58)
    use(p, BASS, "bass/finger", 0xB8)
    use(p, MUTE, "guitar/mute", 0xB8)
    use(p, POWER, "guitar/power-e", 0xB0, reverb=0x30)
    use(p, LEAD, "guitar/clean", 0x98, reverb=0x60, delay=0x40)

    beat = Phrase.merge(Phrase.drums("x.....x.x.......", KICK))
    beat_c = Phrase.drums("x.....x.x.x.....", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    fill = Phrase.drums("....x.......xoxx", SNARE, ghost=0x70)
    hats = Phrase.drums("x.x.x.x.x.x.x.x.", HAT, ghost=0x50)
    ride = Phrase.drums("x.x.x.x.x.x.x.x.", RIDE)
    rests = p.chain([rest()] * 4)

    kicks_v = p.chain([beat] * 4)
    kicks_c = p.chain([beat_c] * 4)
    snares = p.chain([snare, snare, snare, fill])
    hats_c = p.chain([hats] * 4)
    ride_c = p.chain([ride] * 4)
    crash = p.chain([Phrase.parse("C3 . . . . . . . . . . . . . . .", CRASH), Phrase(), Phrase(), Phrase()])
    chugs = p.chain([chug_bar(r) for r in ROOTS])
    powers = p.chain([power_bar(r) for r in ROOTS])
    bass = p.chain([bass_bar(r) for r in ROOTS])
    lead = p.chain([
        Phrase.parse("B3 . . . A3 . . . G3 . . . E3 . . .", LEAD),
        Phrase.parse("G3 . . . . . . . E3 . . . C3 . . .", LEAD),
        Phrase.parse("D3 . . . G3 . . . B3 . . . D4 . . .", LEAD),
        Phrase.parse("C#4 . . . D4 . . . - . . . . . . .", LEAD),
    ])

    rows = [
        # kick    snare   hats/ride  crash   bass   guitar   lead
        [rests, rests, hats_c, rests, rests, chugs, rests],             # intro: chugs
        [kicks_v, snares, hats_c, rests, bass, chugs, rests],           # verse
        [kicks_v, snares, hats_c, rests, bass, chugs, rests],
        [kicks_c, snares, ride_c, crash, bass, powers, lead],           # chorus
        [kicks_c, snares, ride_c, crash, bass, powers, lead],
        [rests, snares, hats_c, rests, bass, chugs, rests],             # breakdown
        [kicks_c, snares, ride_c, crash, bass, powers, lead],           # chorus
        [rests, rests, rests, crash, rests, p.chain([power_bar("E1"), rest(), rest(), rest()]), rests],
    ]
    for i, r in enumerate(rows):
        p.row(i, r + [None])
    return p
