"""BLUE HOUR - small-group jazz swing in Bb, 136 BPM.

Teaching points:
- Swing is the groove: 08 04 makes every second 16th late (long-short).
- The ride plays "ding, ding-da, ding, ding-da"; brushes tap on 2 and 4.
- Walking bass: one note per beat (steps 0 4 8 12), stepping toward the
  next chord's root.
- Piano comps the Charleston rhythm (beat 1 and the "and" of 2).
"""

from lgpt_composer import Phrase, Project
from _kit import chords, notes, use
from _patterns import rest

KICK, RIDE, BRUSH, SWISH, BASS_LOW, BASS_HIGH, MIN7, DOM7, MAJ7, TRUMPET, BELL = range(11)
SLOTS = {"min7": MIN7, "dom7": DOM7, "maj7": MAJ7}
BASS = {"low": BASS_LOW, "high": BASS_HIGH}

# ii V I vi, then IV bVII7 iii VI7 / ii V
A = [("C", "min7"), ("F", "dom7"), ("A#", "maj7"), ("G", "min7")]
B = [("D#", "maj7"), ("G#", "dom7"), ("D", "min7"), ("C", "min7")]


def comp(root: str, quality: str, push: str | None = None) -> Phrase:
    """Charleston: beat 1, the 'and' of 2; the chord is let go on beat 3."""
    hits = [(0, root, quality, 0xB0), (6, root, quality, 0x80)]
    p = chords(SLOTS, hits)
    p.command(10, "KILL", 0)
    if push:  # anticipate the next chord on the last eighth
        nroot, nq = push.split(":")
        p = Phrase.merge(p, chords(SLOTS, [(14, nroot, nq, 0x90)]))
    return p


def build() -> Project:
    p = Project("BlueHour", tempo=136)
    p.key("A#", "Ionian mode (major)")
    p.groove(0, [8, 4])
    p.set("softclip", "Subtle")
    p.set("pregain", 70)
    p.params["reverb size"] = "140"
    p.params["reverb damp"] = "120"

    use(p, KICK, "drums-jazz/kick", 0x50)
    use(p, RIDE, "drums-jazz/ride", 0x58, reverb=0x30)
    use(p, BRUSH, "drums-jazz/brush-tap", 0x80)
    use(p, SWISH, "drums-jazz/brush-swish", 0x58)
    use(p, BASS_LOW, "bass/upright-low", 0xC8)
    use(p, BASS_HIGH, "bass/upright-high", 0xC8)
    for slot, q in ((MIN7, "min7"), (DOM7, "dom7"), (MAJ7, "maj7")):
        use(p, slot, f"chords/piano-{q}", 0x98, reverb=0x40)
    use(p, TRUMPET, "brass/trumpet", 0xA0, reverb=0x50,
        mod1_type="swell", mod1_dest="volume", mod1_amount=-40, mod1_rate=0x70)
    use(p, BELL, "drums-jazz/ride-bell", 0x48)

    ride = Phrase.drums("x...x.x.x...x.x.", RIDE, ghost=0x60)
    ride_bell = Phrase.merge(ride, Phrase.drums("............x...", BELL))
    brush = Phrase.drums("....x.......x...", BRUSH)
    swish = Phrase.drums("x.......x.......", SWISH)
    feather = Phrase.drums("x...x...x...x...", KICK)
    rests = p.chain([rest()] * 4)

    ride_c = p.chain([ride, ride, ride, ride_bell])
    brush_c = p.chain([brush] * 4)
    swish_c = p.chain([swish] * 4)
    kick_c = p.chain([feather] * 4)

    comp_a = p.chain([comp("C", "min7"), comp("F", "dom7"), comp("A#", "maj7"), comp("G", "min7", "C:min7")])
    comp_b = p.chain([comp("D#", "maj7"), comp("G#", "dom7"), comp("D", "min7"), comp("C", "min7", "F:dom7")])

    walk_a = p.chain([
        notes("C2 . . . D2 . . . D#2 . . . E2 . . .", BASS, "A1"),
        notes("F1 . . . A1 . . . C2 . . . D#2 . . .", BASS, "A1"),
        notes("D2 . . . C2 . . . A#1 . . . A1 . . .", BASS, "A1"),
        notes("G1 . . . A#1 . . . D2 . . . C#2 . . .", BASS, "A1"),
    ])
    walk_b = p.chain([
        notes("D#2 . . . D2 . . . C2 . . . A#1 . . .", BASS, "A1"),
        notes("G#1 . . . C2 . . . D#2 . . . C#2 . . .", BASS, "A1"),
        notes("D2 . . . F1 . . . A1 . . . C2 . . .", BASS, "A1"),
        notes("C2 . . . A#1 . . . A1 . . . F1 . . .", BASS, "A1"),
    ])

    head_a = p.chain([
        Phrase.parse(". . G3 . A#3 . D4 . . . C4 . . . A#3 .", TRUMPET),
        Phrase.parse("A3 . . . . . - . C4 . A3 . F3 . . .", TRUMPET),
        Phrase.parse("D4 . . . . . . . . . - . A#3 . C4 .", TRUMPET),
        Phrase.parse("D4 . C4 . A#3 . G3 . . . . . - . . .", TRUMPET),
    ])
    head_b = p.chain([
        Phrase.parse(". . A#3 . D4 . F4 . D#4 . . . D4 . . .", TRUMPET),
        Phrase.parse("C4 . . . . . - . D#4 . C4 . G#3 . . .", TRUMPET),
        Phrase.parse("A3 . . . C4 . . . F4 . . . D4 . . .", TRUMPET),
        Phrase.parse("C4 . . . . . . . A#3 . . . - . . .", TRUMPET),
    ])

    rows = [
        # kick    ride    brush    swish   bass    piano   trumpet
        [rests, ride_c, brush_c, swish_c, walk_a, comp_a, rests],     # intro: rhythm section
        [kick_c, ride_c, brush_c, swish_c, walk_a, comp_a, head_a],   # head A
        [kick_c, ride_c, brush_c, swish_c, walk_b, comp_b, head_b],   # head B
        [kick_c, ride_c, brush_c, swish_c, walk_a, comp_a, rests],    # piano space
        [kick_c, ride_c, brush_c, swish_c, walk_b, comp_b, rests],
        [kick_c, ride_c, brush_c, swish_c, walk_a, comp_a, head_a],   # head again
        [kick_c, ride_c, brush_c, swish_c, walk_b, comp_b, head_b],
        [rests, rests, rests, rests, rests, p.chain([comp("A#", "maj7"), rest(), rest(), rest()]), rests],
    ]
    for i, r in enumerate(rows):
        p.row(i, r + [None])
    return p
