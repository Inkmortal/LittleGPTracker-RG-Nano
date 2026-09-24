"""RAINY WINDOW - lo-fi hip-hop in F major, 78 BPM with swing.

Teaching points:
- Groove 00 is 7 5 instead of 6 6: every other 16th is late = swing.
- Jazz 7th chords from one note with CHRD (min7 037A, dom7 047A, maj7 047B).
- Keys re-hit chords off the beat at lower VOLM, like a lazy piano hand.
- A quiet noise synth plays a constant "vinyl" hiss bed.
"""

from lgpt_composer import Phrase, Project
from _patterns import chord_bar, rest, voice_progression

KICK, SNARE, HAT, BASS, KEYS, FLUTE, VINYL, RIM = range(8)

PROG = ["Gm7", "C7", "Fmaj7", "Dm7"]
PROG_B = ["Bbmaj7", "Am7", "Gm7", "C7"]


def keys_bar(voicing):
    p = chord_bar(voicing, KEYS)
    p.set(10, note=voicing[0], instr=KEYS)
    p.command(10, "CHRD", p.steps[0].param1)
    p.command(10, "VOLM", 0x60)
    return p


def build() -> Project:
    p = Project("RainyWindow", tempo=78)
    p.key("F", "Ionian mode (major)")
    p.groove(0, [7, 5])
    p.set("softclip", "Subtle")
    p.set("pregain", 70)
    p.params["reverb size"] = "170"
    p.params["reverb damp"] = "140"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "80"

    p.synth(KICK, "kick", drive=0x20, decay=0x98, pitch_env=0x80, volume=0xB0)
    p.synth(SNARE, "snare", cutoff=0xB8, decay=0x98, reverb=0x60, volume=0xE0)
    p.synth(HAT, "hat", cutoff=0xB0, decay=0x60, volume=0xD0)
    p.synth(BASS, "subbass", volume=0x44)
    p.synth(KEYS, "keys", cutoff=0xB0, reverb=0x70, lfo_amount=0x18, volume=0x70)
    # Soft breathy lead: plain sine, slow attack, a little vibrato
    p.synth(FLUTE, "lead", wave="sine", attack=0x48, decay=0xB0, sustain=0x98, release=0xA0,
            glide=0x18, cutoff=0x90, reso=0, env_amount=0, lfo_dest="pitch", lfo_rate=0x9C,
            lfo_amount=0x05, reverb=0x80, delay=0x38, volume=0x50)
    p.synth(VINYL, "init", wave="noise", shape=0xF0, sub=0, attack=0x80, decay=0xFF,
            sustain=0xFF, release=0xA0, filter="highpass", cutoff=0xB0, env_amount=0, volume=0x18)
    p.synth(RIM, "perc", volume=0x60, reverb=0x50)

    kick = Phrase.drums("x.....x...x.....", KICK)
    kick_b = Phrase.drums("x.....x...x..x..", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_fill = Phrase.drums("....x.......x..o", SNARE, ghost=0x50)
    hats = Phrase.drums("x.xox.xox.xox.xo", HAT, ghost=0x40)
    rim = Phrase.drums("..........x.....", RIM)
    rests = p.chain([rest()] * 4)

    drums_k = p.chain([kick, kick, kick, kick_b])
    drums_s = p.chain([snare, snare, snare, snare_fill])
    drums_h = p.chain([hats] * 4)
    rim_c = p.chain([rim] * 4)

    vinyl = p.chain([Phrase.parse("C3 . . . . . . . . . . . . . . .", VINYL), Phrase(), Phrase(), Phrase()])
    vinyl_hold = p.chain([Phrase()] * 4)

    v_a = voice_progression(PROG, "C3")
    v_b = voice_progression(PROG_B, "C3")
    keys_a = p.chain([keys_bar(v) for v in v_a])
    keys_b = p.chain([keys_bar(v) for v in v_b])
    keys_intro = p.chain([chord_bar(v, KEYS) for v in v_a])

    # Bass: roots with a lazy pickup (preset tune -24: written 2 octaves up)
    bass_a = p.chain([
        Phrase.parse("G2 . . . . . . . . . G2 . . . F2 .", BASS),
        Phrase.parse("C3 . . . . . . . . . C3 . . . E2 .", BASS),
        Phrase.parse("F2 . . . . . . . . . F2 . . . A2 .", BASS),
        Phrase.parse("D3 . . . . . . . . . D3 . . . C3 .", BASS),
    ])
    bass_b = p.chain([
        Phrase.parse("A#2 . . . . . . . . . A#2 . . . C3 .", BASS),
        Phrase.parse("A2 . . . . . . . . . A2 . . . G2 .", BASS),
        Phrase.parse("G2 . . . . . . . . . G2 . . . A2 .", BASS),
        Phrase.parse("C3 . . . . . . . . . C3 . . . E2 .", BASS),
    ])

    melody_a = p.chain([
        Phrase.parse(". . . . D4 . F4 . . . G4 . . . . .", FLUTE),
        Phrase.parse("E4 . . . . . . . . . . . . . . .", FLUTE),
        Phrase.parse(". . . . C4 . D4 . . . E4 . . . . .", FLUTE),
        Phrase.parse("A3 . . . . . . . . . . . - . . .", FLUTE),
    ])
    melody_a2 = p.chain([
        Phrase.parse(". . . . D4 . F4 . . . A4 . G4 . . .", FLUTE),
        Phrase.parse("E4 . . . . . G4 . . . E4 . . . . .", FLUTE),
        Phrase.parse(". . . . C4 . D4 . . . E4 . C4 . . .", FLUTE),
        Phrase.parse("D4 . . . . . . . . . - . . . . .", FLUTE),
    ])
    melody_b = p.chain([
        Phrase.parse("D4 . . . F4 . . . A4 . . . . . . .", FLUTE),
        Phrase.parse("G4 . . . . . E4 . . . C4 . . . . .", FLUTE),
        Phrase.parse("D4 . . . . . F4 . . . . . . . . .", FLUTE),
        Phrase.parse("E4 . . . . . . . . . - . . . . .", FLUTE),
    ])

    rows = [
        # kick     snare     hats     bass    keys        bell        vinyl       rim
        [rests, rests, rests, rests, keys_intro, rests, vinyl, rests],       # intro
        [drums_k, drums_s, drums_h, bass_a, keys_a, rests, vinyl_hold, rim_c],   # A
        [drums_k, drums_s, drums_h, bass_a, keys_a, melody_a, vinyl_hold, rim_c],  # A + melody
        [drums_k, drums_s, drums_h, bass_b, keys_b, melody_b, vinyl_hold, rim_c],  # B
        [rests, rests, drums_h, bass_a, keys_a, melody_a2, vinyl_hold, rests],     # break
        [drums_k, drums_s, drums_h, bass_a, keys_a, melody_a2, vinyl_hold, rim_c],  # A'
        [drums_k, drums_s, drums_h, bass_b, keys_b, melody_b, vinyl_hold, rim_c],   # B
        [rests, rests, rests, rests, keys_intro, melody_a, vinyl_hold, rests],     # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
