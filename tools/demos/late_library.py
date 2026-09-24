"""LATE LIBRARY - sample-based lo-fi hip-hop in Eb major, 82 BPM with swing.

Teaching points:
- Chords are one-shot samples (a whole piano chord in one WAV): one track
  plays the progression, the note picks the chord's root.
- MOD page on the chords: a slow sine on pitch = tape wobble; a decay on
  cutoff keeps each chord dark after the attack.
- Upright bass and electric piano melody are single-note samples pitched by
  the note column; the vinyl crackle is a sample left looping.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, loop_bar, notes, use
from _patterns import rest

KICK, SNARE, HAT, BASS_LOW, BASS_HIGH, MIN9, DOM7, MAJ7, MIN7, KEYS, VINYL, RIM = range(12)
SLOTS = {"min9": MIN9, "dom7": DOM7, "maj7": MAJ7, "min7": MIN7}
BASS = {"low": BASS_LOW, "high": BASS_HIGH}

# ii - V - I - vi in Eb, then a IV - iii - vi - V turnaround
PROG_A = [("F", "min9"), ("A#", "dom7"), ("D#", "maj7"), ("C", "min7")]
PROG_B = [("G#", "maj7"), ("G", "min7"), ("C", "min9"), ("A#", "dom7")]


def chord_bar(root: str, quality: str, lazy: bool = True) -> Phrase:
    hits = [(0, root, quality)]
    if lazy:
        hits.append((10, root, quality, 0x50))  # a softer re-hit, late in the bar
    return chords(SLOTS, hits)


def build() -> Project:
    p = Project("LateLibrary", tempo=82)
    p.key("D#", "Ionian mode (major)")
    p.groove(0, [7, 5])
    p.set("softclip", "Subtle")
    p.set("pregain", 70)
    p.params["reverb size"] = "160"
    p.params["reverb damp"] = "150"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "70"

    use(p, KICK, "drums-dusty/kick", 0x98)
    use(p, SNARE, "drums-dusty/snare", 0xA8, reverb=0x30)
    use(p, HAT, "drums-dusty/hat", 0x70)
    use(p, BASS_LOW, "bass/upright-low", 0xB0)
    use(p, BASS_HIGH, "bass/upright-high", 0xB0)
    # Tape wobble on the chords: sine on pitch, tiny amount, slow
    wobble = dict(mod2_type="sine", mod2_dest="pitch", mod2_amount=2, mod2_rate=0x38,
                  mod1_type="decay", mod1_dest="cutoff", mod1_amount=40, mod1_rate=0x50,
                  filter_cut=0xC0, reverb=0x50)
    for slot, quality in ((MIN9, "min9"), (DOM7, "dom7"), (MAJ7, "maj7"), (MIN7, "min7")):
        use(p, slot, f"chords/soft-{quality}", 0xF0, **wobble)
    use(p, KEYS, "epiano/note", 0x58, reverb=0x70, delay=0x30,
        mod1_type="sine", mod1_dest="pitch", mod1_amount=2, mod1_rate=0x40)
    use(p, VINYL, "textures/vinyl", 0x90, loopmode="loop")
    use(p, RIM, "drums-dusty/rim", 0x60, reverb=0x40)

    kick = Phrase.drums("x.........x.....", KICK)
    kick_b = Phrase.drums("x.......x.x.....", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_fill = Phrase.drums("....x.......x.oo", SNARE, ghost=0x48)
    hats = Phrase.drums("x.xox.xox.xox.xo", HAT, ghost=0x38)
    rim = Phrase.drums("..........x.....", RIM)
    rests = p.chain([rest()] * 4)

    drums_k = p.chain([kick, kick, kick, kick_b])
    drums_s = p.chain([snare, snare, snare, snare_fill])
    drums_h = p.chain([hats] * 4)
    rim_c = p.chain([rim] * 4)
    vinyl = p.chain([loop_bar(VINYL), Phrase(), Phrase(), Phrase()])
    hold = p.chain([Phrase()] * 4)

    chords_a = p.chain([chord_bar(r, q) for r, q in PROG_A])
    chords_b = p.chain([chord_bar(r, q) for r, q in PROG_B])
    chords_intro = p.chain([chord_bar(r, q, lazy=False) for r, q in PROG_A])

    # Upright bass: root, a push into the next bar, the odd passing note
    bass_a = p.chain([
        notes("F1 . . . . . . . . . F1 . . . . .", BASS, "A1"),
        notes("A#1 . . . . . . . . . A#1 . . . A1 .", BASS, "A1"),
        notes("D#1 . . . . . . . . . D#1 . . . G1 .", BASS, "A1"),
        notes("C2 . . . . . . . . . C2 . . . A#1 .", BASS, "A1"),
    ])
    bass_b = p.chain([
        notes("G#1 . . . . . . . . . G#1 . . . . .", BASS, "A1"),
        notes("G1 . . . . . . . . . G1 . . . . .", BASS, "A1"),
        notes("C2 . . . . . . . . . C2 . . . D2 .", BASS, "A1"),
        notes("A#1 . . . . . . . . . A#1 . . . F1 .", BASS, "A1"),
    ])

    # Electric piano: a short phrase that answers itself
    melody_a = p.chain([
        Phrase.parse(". . . . G3 . A#3 . . . C4 . . . A#3 .", KEYS),
        Phrase.parse("G3 . . . . . . . . . F3 . . . . .", KEYS),
        Phrase.parse(". . . . G3 . A#3 . . . D4 . C4 . . .", KEYS),
        Phrase.parse("A#3 . . . . . . . - . . . . . . .", KEYS),
    ])
    melody_b = p.chain([
        Phrase.parse("C4 . . . D#4 . . . D4 . . . C4 . . .", KEYS),
        Phrase.parse("A#3 . . . . . G3 . . . . . . . . .", KEYS),
        Phrase.parse("G3 . . . A#3 . . . C4 . . . D4 . . .", KEYS),
        Phrase.parse("D#4 . . . . . . . D4 . . . - . . .", KEYS),
    ])

    rows = [
        # kick     snare     hats     bass     chords        keys      vinyl   rim
        [rests, rests, rests, rests, chords_intro, rests, vinyl, rests],      # intro
        [drums_k, drums_s, drums_h, bass_a, chords_a, rests, hold, rim_c],    # A
        [drums_k, drums_s, drums_h, bass_a, chords_a, melody_a, hold, rim_c],  # A + keys
        [drums_k, drums_s, drums_h, bass_b, chords_b, melody_b, hold, rim_c],  # B
        [rests, rests, drums_h, bass_a, chords_a, melody_a, hold, rests],      # break
        [drums_k, drums_s, drums_h, bass_a, chords_a, melody_a, hold, rim_c],  # A
        [drums_k, drums_s, drums_h, bass_b, chords_b, melody_b, hold, rim_c],  # B
        [rests, rests, rests, rests, chords_intro, rests, hold, rests],        # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
