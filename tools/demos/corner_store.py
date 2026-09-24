"""CORNER STORE - boom-bap hip hop in C minor, 90 BPM.

Teaching points:
- Two snares on one hit: the crunchy synth snare and a real old snare drum
  sample, on two tracks, playing the same steps - layering.
- Piano and trumpet as short stabs (one-shot chords and single notes chopped
  short with KILL), the way sample-based beats chop records.
- The kick pattern stumbles ("x..x..x...") against a steady backbeat.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, loop_bar, notes, use
from _patterns import rest

(KICK, SNARE, SNARE_REAL, HAT, BASS_LOW, BASS_HIGH, PIANO_MIN7, PIANO_MAJ7,
 PIANO_DOM7, TRUMPET, VINYL, RISER) = range(12)
SLOTS = {"min7": PIANO_MIN7, "maj7": PIANO_MAJ7, "dom7": PIANO_DOM7}
BASS = {"low": BASS_LOW, "high": BASS_HIGH}


def stab_bar(root: str, quality: str, steps=(0, 6, 10)) -> Phrase:
    p = chords(SLOTS, [(s, root, quality, 0xFF if s == 0 else 0x80) for s in steps])
    for s in steps:
        if s + 2 < 16:
            p.command(s + 2, "KILL", 0)  # chop: every stab is short
    return p


def build() -> Project:
    p = Project("CornerStore", tempo=90)
    p.key("C", "Aeolian mode (minor)")
    p.groove(0, [7, 5])
    p.set("softclip", "Subtle")
    p.set("pregain", 70)
    p.params["reverb size"] = "120"
    p.params["reverb damp"] = "160"

    use(p, KICK, "drums-dusty/kick", 0xB0)
    use(p, SNARE, "drums-dusty/snare", 0x90)
    use(p, SNARE_REAL, "percussion/snare-old", 0x90, reverb=0x28)
    use(p, HAT, "drums-dusty/hat", 0x78)
    use(p, BASS_LOW, "bass/upright-low", 0xC0)
    use(p, BASS_HIGH, "bass/upright-high", 0xC0)
    for slot, quality in ((PIANO_MIN7, "min7"), (PIANO_MAJ7, "maj7"), (PIANO_DOM7, "dom7")):
        use(p, slot, f"chords/piano-{quality}", 0xC0, crush=12, filter_cut=0xB8, reverb=0x30)
    use(p, TRUMPET, "brass/trumpet", 0x98, reverb=0x40)
    use(p, VINYL, "textures/vinyl", 0x98, loopmode="loop")
    use(p, RISER, "textures/riser", 0x70)

    kick = Phrase.drums("x..x..x...x.....", KICK)
    kick_b = Phrase.drums("x..x..x...x..x..", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_real = Phrase.drums("....x.......x...", SNARE_REAL)
    snare_real_fill = Phrase.drums("....x.......x.oo", SNARE_REAL, ghost=0x50)
    hats = Phrase.drums("x.x.x.xox.x.x.xo", HAT, ghost=0x40)
    rests = p.chain([rest()] * 4)

    drums_k = p.chain([kick, kick, kick, kick_b])
    drums_s = p.chain([snare] * 4)
    drums_r = p.chain([snare_real, snare_real, snare_real, snare_real_fill])
    drums_h = p.chain([hats] * 4)
    vinyl = p.chain([loop_bar(VINYL), Phrase(), Phrase(), Phrase()])
    hold = p.chain([Phrase()] * 4)

    # Cm7 Cm7 Abmaj7 G7
    piano = p.chain([stab_bar("C", "min7"), stab_bar("C", "min7", (0, 6)),
                     stab_bar("G#", "maj7"), stab_bar("G", "dom7", (0, 3, 6, 10))])

    bass = p.chain([
        notes("C2 . . . . . C2 . . . D#2 . . . C2 .", BASS, "A1"),
        notes("C2 . . . . . . . . . G1 . . . A#1 .", BASS, "A1"),
        notes("G#1 . . . . . G#1 . . . D#2 . . . G#1 .", BASS, "A1"),
        notes("G1 . . . . . G1 . . . B1 . . . D2 .", BASS, "A1"),
    ])

    # Horn riff: short, punchy trumpet stabs answering the piano
    trumpet = p.chain([
        Phrase.parse("G3 . - . A#3 . - . C4 . . . - . . .", TRUMPET),
        Phrase.parse(". . . . . . . . G3 . A#3 . G3 . - .", TRUMPET),
        Phrase.parse("D#4 . - . D4 . - . C4 . . . - . . .", TRUMPET),
        Phrase.parse(". . . . B3 . - . D4 . - . G3 . - .", TRUMPET),
    ])
    # The last track carries the vinyl loop; the riser takes it over for the
    # break, then the vinyl starts again
    vinyl_back = p.chain([loop_bar(VINYL), Phrase(), Phrase(), Phrase()])
    riser_bar = p.chain([rest(), Phrase(), Phrase(),
                         Phrase.parse("C3 . . . . . . . . . . . . . . .", RISER)])

    rows = [
        # kick    snare    real    hats    bass   piano  trumpet  vinyl/riser
        [rests, rests, rests, drums_h, rests, piano, rests, vinyl],             # intro
        [drums_k, drums_s, drums_r, drums_h, bass, piano, rests, hold],         # verse
        [drums_k, drums_s, drums_r, drums_h, bass, piano, rests, hold],         # verse
        [drums_k, drums_s, drums_r, drums_h, bass, piano, trumpet, hold],       # hook
        [drums_k, drums_s, drums_r, drums_h, bass, piano, trumpet, hold],       # hook
        [rests, rests, rests, drums_h, bass, piano, rests, riser_bar],          # break
        [drums_k, drums_s, drums_r, drums_h, bass, piano, trumpet, vinyl_back],  # hook
        [rests, rests, rests, rests, rests, piano, rests, hold],                # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
