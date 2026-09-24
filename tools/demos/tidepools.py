"""TIDEPOOLS - ambient in F (Lydian colour), 66 BPM.

Teaching points:
- Space is the instrument: long string chords, big reverb, a delay on the
  harp and glock so every note repeats and fades.
- Electric piano 7th and 9th chords (one-shots) float over a cello drone.
- A slow MOD LFO on the pad's volume makes it swell like waves.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, loop_bar, notes, use
from _patterns import rest

DRONE, STR_MAJ, STR_MIN, EP_MAJ7, EP_MAJ9, EP_MIN7, HARP_LOW, HARP_HIGH, GLOCK, HISS, MARIMBA = range(11)
PAD = {"maj": STR_MAJ, "min": STR_MIN}
EP = {"maj7": EP_MAJ7, "maj9": EP_MAJ9, "min7": EP_MIN7}
HARP = {"low": HARP_LOW, "high": HARP_HIGH}

PROG = [("F", "maj9"), ("G", "maj7"), ("A", "min7"), ("F", "maj7")]  # G major chord = the Lydian #4
PADS = [("F", "maj"), ("G", "maj"), ("A", "min"), ("C", "maj")]


def harp_bar(tones: str) -> Phrase:
    parts = tones.split()
    text = []
    for i in range(16):
        text.append(parts[i // 4] if i % 4 == 0 else ".")
    return notes(" ".join(text), HARP, "C3")


def build() -> Project:
    p = Project("Tidepools", tempo=66)
    p.key("F", "Lydian mode")
    p.set("softclip", "Subtle")
    p.set("pregain", 66)
    p.params["reverb size"] = "250"
    p.params["reverb damp"] = "80"
    p.params["delay steps"] = "6"
    p.params["delay feedback"] = "130"

    use(p, DRONE, "strings/cellos-low", 0x80, reverb=0x90)
    swell = dict(mod1_type="sine", mod1_dest="volume", mod1_amount=-50, mod1_rate=0x20, reverb=0xC0)
    use(p, STR_MAJ, "chords/strings-maj", 0x90, **swell)
    use(p, STR_MIN, "chords/strings-min", 0x90, **swell)
    for slot, q in ((EP_MAJ7, "maj7"), (EP_MAJ9, "maj9"), (EP_MIN7, "min7")):
        use(p, slot, f"epiano/{q}", 0x70, reverb=0xA0, delay=0x30)
    use(p, HARP_LOW, "keys/harp-low", 0x78, reverb=0xA0, delay=0x60)
    use(p, HARP_HIGH, "keys/harp-high", 0x78, reverb=0xA0, delay=0x60)
    use(p, GLOCK, "keys/glock", 0x48, reverb=0xC0, delay=0x80)
    use(p, HISS, "textures/tape-hiss", 0x50, loopmode="loop")
    use(p, MARIMBA, "keys/marimba", 0x60, reverb=0xA0, delay=0x50)

    rests = p.chain([rest()] * 4)
    drone = p.chain([notes("F1 . . . . . . . . . . . . . . .", DRONE), Phrase(),
                     notes("F1 . . . . . . . . . . . . . . .", DRONE), Phrase()])
    pads = p.chain([chords(PAD, [(0, r, q)]) for r, q in PADS])
    keys = p.chain([chords(EP, [(0, r, q), (10, r, q, 0x60)]) for r, q in PROG])
    harp = p.chain([harp_bar("F2 C3 A3 E3"), harp_bar("G2 D3 B3 F#3"),
                    harp_bar("A2 E3 C4 G3"), harp_bar("F2 C3 E3 A3")])
    glock = p.chain([
        Phrase.parse(". . . . . . . . E4 . . . . . . .", GLOCK),
        Phrase.parse(". . . . B4 . . . . . . . . . . .", GLOCK),
        Phrase.parse(". . . . . . . . . . . . C5 . . .", GLOCK),
        Phrase.parse(". . A4 . . . . . . . . . . . . .", GLOCK),
    ])
    marimba = p.chain([
        Phrase.parse("C4 . . . . . A3 . . . . . . . . .", MARIMBA),
        Phrase.parse("B3 . . . . . D4 . . . . . . . . .", MARIMBA),
        Phrase.parse("E4 . . . . . C4 . . . . . . . . .", MARIMBA),
        Phrase.parse("A3 . . . . . . . . . . . - . . .", MARIMBA),
    ])
    hiss = p.chain([loop_bar(HISS), Phrase(), Phrase(), Phrase()])
    hold = p.chain([Phrase()] * 4)

    rows = [
        # drone   pads    keys    harp    glock   marimba  hiss
        [drone, pads, rests, rests, rests, rests, hiss],
        [drone, pads, keys, rests, glock, rests, hold],
        [drone, pads, keys, harp, glock, rests, hold],
        [drone, pads, keys, harp, glock, marimba, hold],
        [drone, pads, rests, harp, rests, marimba, hold],
        [drone, pads, keys, harp, glock, marimba, hold],
        [drone, pads, keys, rests, glock, rests, hold],
        [drone, pads, rests, rests, rests, rests, hold],
    ]
    for i, r in enumerate(rows):
        p.row(i, r + [None])
    return p
