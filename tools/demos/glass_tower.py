"""GLASS TOWER - trap in C minor, 140 BPM (half-time feel).

Teaching points:
- The 808 kick is also the bass: it is a sample with a root note, so the
  note column tunes it (C1, G#0, F0, G0 follow the chords).
- Hi-hat rolls: RTRG 0003 retriggers the hat every 3 ticks for a burst.
- Half-time: the clap lands on step 08, once per bar, so 140 BPM feels slow.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, notes, use
from _patterns import rest

BOOM, CLAP, HAT, OPEN, MIN, MAJ, GLOCK, PIANO, RISER = range(9)
PAD = {"min": MIN, "maj": MAJ}


def hat_bar(rolls: dict[int, int]) -> Phrase:
    p = Phrase.drums("x.x.x.x.x.x.x.x.", HAT, ghost=0x50)
    for step, param in rolls.items():
        p.set(step, note=60, instr=HAT)
        p.command(step, "RTRG", param)
    return p


def build() -> Project:
    p = Project("GlassTower", tempo=140)
    p.key("C", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.set("pregain", 68)
    p.params["reverb size"] = "200"
    p.params["reverb damp"] = "110"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "90"

    use(p, BOOM, "drums-808/kick", 0xC8, root="F0")
    use(p, CLAP, "drums-808/clap", 0xB0, reverb=0x40)
    use(p, HAT, "drums-808/hat", 0x88)
    use(p, OPEN, "drums-808/openhat", 0x70)
    use(p, MIN, "chords/strings-min", 0x78, reverb=0x60, filter_cut=0xA0)
    use(p, MAJ, "chords/strings-maj", 0x78, reverb=0x60, filter_cut=0xA0)
    use(p, GLOCK, "keys/glock", 0x70, reverb=0x70, delay=0x50)
    use(p, PIANO, "keys/piano-high", 0x88, reverb=0x60)
    use(p, RISER, "textures/riser", 0x60)

    # Cm Ab Fm G
    booms = p.chain([
        Phrase.parse("C1 . . . . . . C1 . . C1 . . . . .", BOOM),
        Phrase.parse("G#0 . . . . . . G#0 . . . . . . D#1 .", BOOM),
        Phrase.parse("F0 . . . . . . F0 . . F0 . . . . .", BOOM),
        Phrase.parse("G0 . . . . . . G0 . . . . G0 . A#0 .", BOOM),
    ])
    clap = Phrase.drums("........x.......", CLAP)
    clap_fill = Phrase.drums("........x.....x.", CLAP)
    claps = p.chain([clap, clap, clap, clap_fill])
    hats = p.chain([hat_bar({6: 3}), hat_bar({14: 2}), hat_bar({6: 3, 11: 3}), hat_bar({12: 2, 14: 1})])
    opens = p.chain([Phrase.drums("..............x.", OPEN)] * 4)
    pads = p.chain([chords(PAD, [(0, "C", "min")]), chords(PAD, [(0, "G#", "maj")]),
                    chords(PAD, [(0, "F", "min")]), chords(PAD, [(0, "G", "maj")])])
    rests = p.chain([rest()] * 4)

    # A glass-bell motif, echoed by the delay
    glock = p.chain([
        Phrase.parse("G4 . . D#4 . . C4 . . . . . D#4 . . .", GLOCK),
        Phrase.parse("C4 . . . . . . . - . . . . . . .", GLOCK),
        Phrase.parse("G#4 . . F4 . . C4 . . . . . F4 . . .", GLOCK),
        Phrase.parse("D4 . . . . . B3 . . . . . - . . .", GLOCK),
    ])
    piano = p.chain([
        Phrase.parse("C3 . . . . . . . D#3 . . . . . . .", PIANO),
        Phrase.parse("C3 . . . . . . . G2 . . . . . . .", PIANO),
        Phrase.parse("C3 . . . . . . . G#2 . . . . . . .", PIANO),
        Phrase.parse("B2 . . . . . . . D3 . . . - . . .", PIANO),
    ])
    riser = p.chain([Phrase(), Phrase(), Phrase(), Phrase.parse("C3 . . . . . . . . . . . . . . .", RISER)])

    rows = [
        # 808     clap    hats     open    pad     glock   piano   fx
        [rests, rests, rests, rests, pads, rests, piano, riser],     # intro
        [booms, claps, hats, opens, pads, rests, piano, rests],      # verse
        [booms, claps, hats, opens, pads, glock, piano, rests],      # hook
        [booms, claps, hats, opens, pads, glock, piano, riser],      # hook
        [rests, rests, hats, rests, pads, glock, rests, rests],      # break
        [booms, claps, hats, opens, pads, glock, piano, rests],      # hook
        [booms, claps, hats, opens, pads, glock, piano, rests],
        [rests, rests, rests, rests, pads, rests, piano, rests],     # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
