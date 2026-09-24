"""SUNSET CLUB - house in A minor, 124 BPM.

Teaching points:
- Four on the floor: the kick on every beat (steps 0 4 8 12), the clap on
  2 and 4, the open hat on every offbeat (2 6 10 14).
- The classic house piano: one-shot min7/maj7 chords chopped into stabs
  with a syncopated rhythm, cut short by KILL.
- The bass plays the offbeats, between the kicks, so they never clash.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, notes, use
from _patterns import rest

KICK, CLAP, HAT, OPEN, BASS, MIN7, MAJ7, STR_MIN, STR_MAJ, RISER, CRASH = range(11)
PIANO = {"min7": MIN7, "maj7": MAJ7}
PAD = {"min": STR_MIN, "maj": STR_MAJ}

PROG = [("A", "min7"), ("F", "maj7"), ("D", "min7"), ("E", "min7")]
STABS = (0, 3, 6, 10, 13)


def stab_bar(root: str, quality: str) -> Phrase:
    p = chords(PIANO, [(s, root, quality, 0xFF if s in (0, 6) else 0x98) for s in STABS])
    for s in STABS:
        if s + 2 < 16 and s + 2 not in STABS:
            p.command(s + 2, "KILL", 0)
    return p


def bass_bar(root: str) -> Phrase:
    low = f"{root}1"
    high = f"{root}2"
    return Phrase.parse(f". . {low} . . . {high} . . . {low} . . . {high} .", BASS)


def build() -> Project:
    p = Project("SunsetClub", tempo=124)
    p.key("A", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.set("pregain", 66)
    p.params["reverb size"] = "150"
    p.params["reverb damp"] = "100"

    use(p, KICK, "drums-909/kick", 0xD0)
    use(p, CLAP, "drums-909/clap", 0xA0, reverb=0x40)
    use(p, HAT, "drums-909/hat", 0x80)
    use(p, OPEN, "drums-909/openhat", 0x78)
    use(p, BASS, "bass/finger", 0xC0)
    use(p, MIN7, "chords/piano-min7", 0xC8, reverb=0x50)
    use(p, MAJ7, "chords/piano-maj7", 0xC8, reverb=0x50)
    use(p, STR_MIN, "chords/strings-min", 0x80, reverb=0x70)
    use(p, STR_MAJ, "chords/strings-maj", 0x80, reverb=0x70)
    use(p, RISER, "textures/riser", 0x70)
    use(p, CRASH, "drums-909/crash", 0x70)

    four = Phrase.drums("x...x...x...x...", KICK)
    clap = Phrase.drums("....x.......x...", CLAP)
    hats = Phrase.drums("x.x.x.x.x.x.x.x.", HAT, ghost=0x48)
    hats16 = Phrase.drums("xoxoxoxoxoxoxoxo", HAT, ghost=0x40)
    opens = Phrase.drums("..x...x...x...x.", OPEN)
    rests = p.chain([rest()] * 4)

    kicks = p.chain([four] * 4)
    claps = p.chain([clap] * 4)
    hats_c = p.chain([hats, hats, hats, hats16])
    opens_c = p.chain([opens] * 4)
    piano = p.chain([stab_bar(r, q) for r, q in PROG])
    bass = p.chain([bass_bar("A"), bass_bar("F"), bass_bar("D"), bass_bar("E")])
    pads = p.chain([chords(PAD, [(0, "A", "min")]), chords(PAD, [(0, "F", "maj")]),
                    chords(PAD, [(0, "D", "min")]), chords(PAD, [(0, "E", "min")])])
    fx_in = p.chain([Phrase.parse("C3 . . . . . . . . . . . . . . .", CRASH), Phrase(), Phrase(), Phrase()])
    fx_rise = p.chain([Phrase(), Phrase(), Phrase(), Phrase.parse("C3 . . . . . . . . . . . . . . .", RISER)])

    rows = [
        # kick    clap    hats    open    bass    piano   pads    fx
        [kicks, rests, hats_c, rests, rests, rests, rests, rests],      # intro: kick and hats
        [kicks, claps, hats_c, opens_c, bass, rests, rests, rests],     # groove
        [kicks, claps, hats_c, opens_c, bass, piano, rests, fx_in],     # piano enters
        [kicks, claps, hats_c, opens_c, bass, piano, rests, rests],
        [rests, rests, rests, rests, rests, piano, pads, fx_rise],      # breakdown
        [kicks, claps, hats_c, opens_c, bass, piano, pads, fx_in],      # drop
        [kicks, claps, hats_c, opens_c, bass, piano, pads, rests],
        [kicks, rests, hats_c, rests, rests, rests, pads, rests],       # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
