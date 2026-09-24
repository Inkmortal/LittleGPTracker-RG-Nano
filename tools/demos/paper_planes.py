"""PAPER PLANES - pop in C major, 108 BPM.

Teaching points:
- The I - V - vi - IV progression (C G Am F) under almost every pop song.
- Piano chords as a steady eighth-note pulse: accents on the beat, softer
  in between (VOLM), so it moves without getting busy.
- Verse and chorus use the same chords; the chorus adds the hook, the glock
  doubling it an octave up, and a busier drum pattern.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, use
from _patterns import rest

KICK, CLAP, SHAKER, BASS, MAJ, MIN, LEAD, GLOCK, CRASH = range(9)
PIANO = {"maj": MAJ, "min": MIN}
PROG = [("C", "maj"), ("G", "maj"), ("A", "min"), ("F", "maj")]


def pulse_bar(root: str, quality: str) -> Phrase:
    return chords(PIANO, [(s, root, quality, 0xC8 if s % 4 == 0 else 0x70) for s in range(0, 16, 2)])


def bass_bar(root: str, walk: str | None = None) -> Phrase:
    text = f"{root}1 . . . . . . {root}1 . . {root}1 . . . {walk or root + '1'} ."
    return Phrase.parse(text, BASS)


def build() -> Project:
    p = Project("PaperPlanes", tempo=108)
    p.key("C", "Ionian mode (major)")
    p.set("softclip", "Subtle")
    p.set("pregain", 66)
    p.params["reverb size"] = "150"
    p.params["reverb damp"] = "110"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "70"

    use(p, KICK, "drums-909/kick", 0xB8)
    use(p, CLAP, "drums-808/clap", 0xA8, reverb=0x50)
    use(p, SHAKER, "percussion/shaker", 0x70)
    use(p, BASS, "bass/finger", 0xC0)
    use(p, MAJ, "chords/piano-maj", 0xB0, reverb=0x40)
    use(p, MIN, "chords/piano-min", 0xB0, reverb=0x40)
    p.synth(LEAD, "pluck", cutoff=0x70, env_amount=0xA0, reverb=0x50, delay=0x40, volume=0x70)
    use(p, GLOCK, "keys/glock", 0x58, reverb=0x70)
    use(p, CRASH, "drums-909/crash", 0x60)

    verse_kick = Phrase.drums("x.......x.......", KICK)
    chorus_kick = Phrase.drums("x.....x.x...x...", KICK)
    clap = Phrase.drums("....x.......x...", CLAP)
    shaker = Phrase.drums("xoxoxoxoxoxoxoxo", SHAKER, ghost=0x40)
    rests = p.chain([rest()] * 4)

    kicks_v = p.chain([verse_kick] * 4)
    kicks_c = p.chain([chorus_kick] * 4)
    claps = p.chain([clap] * 4)
    shakers = p.chain([shaker] * 4)
    piano = p.chain([pulse_bar(r, q) for r, q in PROG])
    bass = p.chain([bass_bar("C", "B0"), bass_bar("G"), bass_bar("A", "G1"), bass_bar("F")])

    verse_line = p.chain([
        Phrase.parse("E4 . . . G4 . . . G4 . E4 . D4 . . .", LEAD),
        Phrase.parse("D4 . . . . . . . - . . . . . . .", LEAD),
        Phrase.parse("C4 . . . E4 . . . E4 . D4 . C4 . . .", LEAD),
        Phrase.parse("A3 . . . . . . . - . . . . . . .", LEAD),
    ])
    hook = [
        "G4 . . . G4 . A4 . G4 . . . E4 . . .",
        "D4 . . . D4 . E4 . D4 . . . B3 . . .",
        "C4 . . . E4 . . . A4 . . . G4 . . .",
        "F4 . . . E4 . . . D4 . C4 . - . . .",
    ]
    chorus = p.chain([Phrase.parse(h, LEAD) for h in hook])
    glock_hook = p.chain([Phrase.parse(h, GLOCK) for h in hook])
    crash = p.chain([Phrase.parse("C3 . . . . . . . . . . . . . . .", CRASH), Phrase(), Phrase(), Phrase()])

    rows = [
        # kick    clap    shaker   bass   piano   lead     glock    crash
        [rests, rests, rests, rests, piano, rests, rests, rests],           # intro
        [kicks_v, claps, shakers, bass, piano, verse_line, rests, rests],   # verse
        [kicks_v, claps, shakers, bass, piano, verse_line, rests, rests],
        [kicks_c, claps, shakers, bass, piano, chorus, glock_hook, crash],  # chorus
        [kicks_c, claps, shakers, bass, piano, chorus, glock_hook, rests],
        [rests, claps, shakers, bass, piano, verse_line, rests, rests],     # bridge
        [kicks_c, claps, shakers, bass, piano, chorus, glock_hook, crash],  # chorus
        [rests, rests, rests, rests, piano, rests, glock_hook, rests],      # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
