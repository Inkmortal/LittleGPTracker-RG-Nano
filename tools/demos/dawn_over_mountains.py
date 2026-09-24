"""DAWN OVER MOUNTAINS - cinematic orchestral in D major, 84 BPM.

Teaching points:
- An orchestra is layers: a bass line (cellos), a harmony bed (string
  chords), a melody (violins), a counter-line (horn), motion (harp arpeggios)
  and colour (glockenspiel, timpani, cymbal).
- Two recordings of the violins cover the range: notes from E4 up use the
  higher one, so nothing is pitched far from where it was played.
- Timpani rolls are one hit retriggered fast with RTRG, swelling with VOLM.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, notes, use
from _patterns import rest

(TIMP, CELLO_LOW, CELLO, STR_MAJ, STR_MIN, VLN_LOW, VLN_HIGH, HORN,
 HARP_LOW, HARP_HIGH, GLOCK, CRASH) = range(12)
PAD = {"maj": STR_MAJ, "min": STR_MIN}
VIOLINS = {"low": VLN_LOW, "high": VLN_HIGH}
CELLOS = {"low": CELLO_LOW, "high": CELLO}
HARP = {"low": HARP_LOW, "high": HARP_HIGH}

# D A Bm G
PROG = [("D", "maj"), ("A", "maj"), ("B", "min"), ("G", "maj")]
ARPS = {"D": "D2 A2 D3 F#3 A3 F#3 D3 A2", "A": "A1 E2 A2 C#3 E3 C#3 A2 E2",
        "B": "B1 F#2 B2 D3 F#3 D3 B2 F#2", "G": "G1 D2 G2 B2 D3 B2 G2 D2"}


def harp_bar(chord: str) -> Phrase:
    tones = ARPS[chord].split()
    text = " ".join(f"{t} ." for t in tones)
    return notes(text, HARP, "C3")


def timp_roll() -> Phrase:
    p = Phrase().set(8, note=60, instr=TIMP)
    p.command(8, "RTRG", 0x0002)
    p.command(8, "VOLM", 0x40)
    for i, vol in zip((10, 12, 14), (0x70, 0xA0, 0xE0)):
        p.set(i, cmd1="VOLM", param1=vol)
    return p


def build() -> Project:
    p = Project("DawnOverMountains", tempo=84)
    p.key("D", "Ionian mode (major)")
    p.set("softclip", "Subtle")
    p.set("pregain", 64)
    p.params["reverb size"] = "230"
    p.params["reverb damp"] = "90"

    use(p, TIMP, "percussion/timpani", 0xB0, reverb=0x60)
    use(p, CELLO_LOW, "strings/cellos-low", 0xA8, reverb=0x60)
    use(p, CELLO, "strings/cellos", 0xA8, reverb=0x60)
    use(p, STR_MAJ, "chords/strings-maj", 0x80, reverb=0x80)
    use(p, STR_MIN, "chords/strings-min", 0x80, reverb=0x80)
    use(p, VLN_LOW, "strings/violins-low", 0xA0, reverb=0x80)
    use(p, VLN_HIGH, "strings/violins-high", 0xA0, reverb=0x80)
    use(p, HORN, "brass/horn", 0x98, reverb=0x80)
    use(p, HARP_LOW, "keys/harp-low", 0x90, reverb=0x70)
    use(p, HARP_HIGH, "keys/harp-high", 0x90, reverb=0x70)
    use(p, GLOCK, "keys/glock", 0x60, reverb=0x90)
    use(p, CRASH, "percussion/crash", 0x78, reverb=0x60)

    rests = p.chain([rest()] * 4)
    timp_hit = Phrase.parse("C3 . . . . . . . . . . . . . . .", TIMP)
    timps = p.chain([timp_hit, Phrase(), timp_hit, timp_roll()])
    cellos = p.chain([
        notes("D2 . . . . . . . . . . . . . . .", CELLOS, "C3"),
        notes("A1 . . . . . . . . . . . . . . .", CELLOS, "C3"),
        notes("B1 . . . . . . . . . . . . . . .", CELLOS, "C3"),
        notes("G1 . . . . . . . . . . . . . . .", CELLOS, "C3"),
    ])
    pads = p.chain([chords(PAD, [(0, r, q)]) for r, q in PROG])
    harp = p.chain([harp_bar(c) for c in ("D", "A", "B", "G")])
    theme = p.chain([
        notes("F#3 . . . . . . . A3 . . . D4 . . .", VIOLINS, "E4"),
        notes("C#4 . . . . . . . . . . . B3 . A3 .", VIOLINS, "E4"),
        notes("B3 . . . . . . . D4 . . . F#4 . . .", VIOLINS, "E4"),
        notes("E4 . . . . . . . D4 . . . . . . .", VIOLINS, "E4"),
    ])
    theme_high = p.chain([
        notes("A4 . . . . . . . F#4 . . . D4 . . .", VIOLINS, "E4"),
        notes("E4 . . . . . . . C#4 . . . E4 . . .", VIOLINS, "E4"),
        notes("F#4 . . . . . . . D4 . . . B3 . . .", VIOLINS, "E4"),
        notes("D4 . . . . . . . . . . . . . . .", VIOLINS, "E4"),
    ])
    horn = p.chain([
        Phrase.parse("D2 . . . . . . . . . . . F#2 . . .", HORN),
        Phrase.parse("E2 . . . . . . . . . . . . . . .", HORN),
        Phrase.parse("F#2 . . . . . . . . . . . D2 . . .", HORN),
        Phrase.parse("D2 . . . . . . . B1 . . . . . . .", HORN),
    ])
    glock = p.chain([
        Phrase.parse(". . . . . . . . A4 . . . . . . .", GLOCK),
        Phrase.parse(". . . . . . . . E4 . . . . . . .", GLOCK),
        Phrase.parse(". . . . . . . . F#4 . . . . . . .", GLOCK),
        Phrase.parse(". . . . D4 . . . . . . . B3 . . .", GLOCK),
    ])
    crash = p.chain([Phrase.parse("C3 . . . . . . . . . . . . . . .", CRASH), Phrase(), Phrase(), Phrase()])

    rows = [
        # timp   cellos   pads    violins     horn   harp   glock   crash
        [rests, cellos, pads, rests, rests, harp, rests, rests],          # dawn: strings and harp
        [timps, cellos, pads, theme, rests, harp, rests, rests],          # the theme
        [timps, cellos, pads, theme, horn, harp, glock, rests],           # horn joins
        [timps, cellos, pads, theme_high, horn, harp, glock, crash],      # climax
        [rests, cellos, pads, rests, horn, harp, glock, rests],           # breath
        [timps, cellos, pads, theme_high, horn, harp, glock, crash],      # climax again
        [rests, cellos, pads, theme, rests, harp, rests, rests],          # fading
        [rests, rests, pads, rests, rests, harp, glock, rests],           # last light
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
