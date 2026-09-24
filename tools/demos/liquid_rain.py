"""LIQUID RAIN - liquid drum & bass in D minor, 174 BPM.

Teaching points:
- The two-step break: kick on 0 and 10, snare on 4 and 12, quiet ghost
  snares between them - that push is what makes it roll.
- Reese bass: one long detuned note per chord, a MOD LFO slowly moving its
  filter so it breathes.
- At 174 BPM a bar is short, so pads hold across it and the piano plays sparse.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, use
from _patterns import rest

KICK, SNARE, HAT, RIDE, REESE, STR_MIN, STR_MAJ, PIANO, RISER = range(9)
PAD = {"min": STR_MIN, "maj": STR_MAJ}


def build() -> Project:
    p = Project("LiquidRain", tempo=174)
    p.key("D", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.set("pregain", 66)
    p.params["reverb size"] = "210"
    p.params["reverb damp"] = "120"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "90"

    use(p, KICK, "drums-909/kick", 0xB8)
    use(p, SNARE, "drums-dusty/snare", 0xC8, reverb=0x30)
    use(p, HAT, "drums-909/hat", 0x70)
    use(p, RIDE, "drums-909/ride", 0x50)
    use(p, REESE, "bass/reese", 0xB8, filter_cut=0x90,
        mod1_type="sine", mod1_dest="cutoff", mod1_amount=30, mod1_rate=0x30)
    use(p, STR_MIN, "chords/strings-min", 0x78, reverb=0x80)
    use(p, STR_MAJ, "chords/strings-maj", 0x78, reverb=0x80)
    use(p, PIANO, "keys/piano-high", 0x90, reverb=0x80, delay=0x40)
    use(p, RISER, "textures/riser", 0x60)

    kick = Phrase.drums("x.........x.....", KICK)
    kick_b = Phrase.drums("x.........x..x..", KICK)
    snare = Phrase.drums("....x.o.....x..o", SNARE, ghost=0x40)
    snare_b = Phrase.drums("....x.o.....x.oo", SNARE, ghost=0x48)
    hats = Phrase.drums("x.xox.xox.xox.xo", HAT, ghost=0x40)
    ride = Phrase.drums("x...x...x...x...", RIDE)
    rests = p.chain([rest()] * 4)

    kicks = p.chain([kick, kick, kick, kick_b])
    snares = p.chain([snare, snare, snare, snare_b])
    hats_c = p.chain([hats] * 4)
    ride_c = p.chain([ride] * 4)
    # Dm Bb F C
    reese = p.chain([
        Phrase.parse("D1 . . . . . . . . . . . . . . .", REESE),
        Phrase.parse("A#0 . . . . . . . . . . . . . . .", REESE),
        Phrase.parse("F0 . . . . . . . . . . . . . . .", REESE),
        Phrase.parse("C1 . . . . . . . . . . . . . . .", REESE),
    ])
    pads = p.chain([chords(PAD, [(0, "D", "min")]), chords(PAD, [(0, "A#", "maj")]),
                    chords(PAD, [(0, "F", "maj")]), chords(PAD, [(0, "C", "maj")])])
    piano = p.chain([
        Phrase.parse("A3 . . . . . F3 . . . . . D3 . . .", PIANO),
        Phrase.parse("D3 . . . . . . . . . F3 . . . . .", PIANO),
        Phrase.parse("C4 . . . . . A3 . . . . . F3 . . .", PIANO),
        Phrase.parse("E3 . . . . . . . G3 . . . - . . .", PIANO),
    ])
    riser = p.chain([Phrase(), Phrase(), Phrase(), Phrase.parse("C3 . . . . . . . . . . . . . . .", RISER)])

    rows = [
        # kick    snare    hats    ride    reese   pads    piano   fx
        [rests, rests, rests, rests, rests, pads, piano, riser],       # intro
        [kicks, snares, hats_c, rests, reese, pads, rests, rests],     # drop
        [kicks, snares, hats_c, ride_c, reese, pads, piano, rests],
        [kicks, snares, hats_c, ride_c, reese, pads, piano, riser],
        [rests, rests, hats_c, rests, rests, pads, piano, rests],      # break
        [rests, rests, hats_c, rests, reese, pads, piano, riser],       # build
        [kicks, snares, hats_c, ride_c, reese, pads, piano, rests],    # drop 2
        [kicks, snares, hats_c, ride_c, reese, pads, piano, rests],
        [rests, rests, rests, ride_c, reese, pads, piano, rests],      # breather
        [rests, rests, hats_c, ride_c, reese, pads, piano, riser],
        [kicks, snares, hats_c, ride_c, reese, pads, piano, rests],    # drop 3
        [kicks, snares, hats_c, ride_c, reese, pads, piano, rests],
        [rests, rests, rests, rests, rests, pads, piano, rests],       # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
