"""PIXEL QUEST - chiptune in C major, 140 BPM.

Teaching points:
- ARPG turns one held note into a fast arpeggio: the classic 8-bit chord.
  ARPG 0047 cycles root, +4, +7 (major), ARPG 0037 is minor.
- Drums built from a triangle with a pitch drop (kick) and crunchy noise.
- The echo lead is the same melody on another track with a delay send and
  a DLAY command: a cheap stereo "chorus" trick.
"""

from lgpt_composer import Phrase, Project
from _patterns import octave_bass, rest

KICK, SNARE, HAT, BASS, LEAD, ARP, ECHO = range(7)

PROG = [("C3", 0x0047), ("G2", 0x0047), ("A2", 0x0037), ("F2", 0x0047)]


def arp_bar(root, arpg):
    p = Phrase.parse(f"{root} . . . . . . . . . . . . . . .", ARP)
    p.command(0, "ARPG", arpg)
    return p


def build() -> Project:
    p = Project("PixelQuest", tempo=140)
    p.key("C", "Ionian mode (major)")
    p.set("softclip", "Subtle")
    p.set("pregain", 60)
    p.params["reverb size"] = "110"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "70"

    p.synth(KICK, "kick", wave="triangle", tune=-20, pitch_env=0x70, pitch_decay=0x60,
            decay=0x80, drive=0x30, volume=0xC0)
    p.synth(SNARE, "snare", wave="noise", shape=0x40, noise=0, decay=0x88, filter="off", volume=0xC8)
    p.synth(HAT, "hat", wave="noise", shape=0x20, noise=0, decay=0x58, filter="highpass",
            cutoff=0xA0, volume=0x98)
    p.synth(BASS, "chip", shape=0x00, tune=-12, volume=0x58)
    p.synth(LEAD, "chip", shape=0x40, lfo_dest="pitch", lfo_rate=0xB8, lfo_amount=0x0C,
            glide=0x18, volume=0x58)
    p.synth(ARP, "chip", shape=0x90, decay=0xB0, sustain=0x80, volume=0x3C, pan=0x60)
    p.synth(ECHO, "chip", shape=0x60, delay=0x80, pan=0xA0, volume=0x30)

    kick = Phrase.drums("x...x...x...x...", KICK)
    kick_fill = Phrase.drums("x...x...x.x.xxxx", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    hats = Phrase.drums("xoxoxoxoxoxoxoxo", HAT, ghost=0x30)
    rests = p.chain([rest()] * 4)

    drums_k = p.chain([kick, kick, kick, kick_fill])
    drums_s = p.chain([snare] * 4)
    drums_h = p.chain([hats] * 4)
    bass = p.chain([octave_bass("C3", BASS), octave_bass("G2", BASS),
                    octave_bass("A2", BASS), octave_bass("F2", BASS)])
    arps = p.chain([arp_bar(n, a) for n, a in PROG])

    verse = [
        "E4 . G4 . C5 . . . B4 . G4 . E4 . . .",
        "D4 . . . G4 . . . B4 . A4 . G4 . . .",
        "C5 . . . B4 . A4 . E4 . . . A4 . . .",
        "A4 . G4 . F4 . E4 . F4 . . . G4 . . .",
    ]
    chorus = [
        "G4 . . . C5 . . . E5 . D5 . C5 . . .",
        "B4 . . . D5 . . . G4 . . . . . . .",
        "A4 . . . C5 . B4 . A4 . G4 . E4 . . .",
        "F4 . . . A4 . . . G4 . . . - . . .",
    ]
    lead_v = p.chain([Phrase.parse(t, LEAD) for t in verse])
    lead_c = p.chain([Phrase.parse(t, LEAD) for t in chorus])
    echo_c = []
    for t in chorus:
        ph = Phrase.parse(t, ECHO)
        for i, s in enumerate(ph.steps):
            if s.note != 0xFF:
                ph.command(i, "DLAY", 0x0003)
        echo_c.append(ph)
    echo_c = p.chain(echo_c)

    rows = [
        # kick     snare    hats     bass   lead    arp    echo
        [rests, rests, rests, bass, rests, arps, rests, rests],        # intro
        [drums_k, drums_s, drums_h, bass, rests, arps, rests, rests],  # groove
        [drums_k, drums_s, drums_h, bass, lead_v, arps, rests, rests],  # verse
        [drums_k, drums_s, drums_h, bass, lead_c, arps, echo_c, rests],  # chorus
        [rests, drums_s, drums_h, bass, lead_v, arps, rests, rests],    # breakdown
        [drums_k, drums_s, drums_h, bass, lead_v, arps, rests, rests],  # verse
        [drums_k, drums_s, drums_h, bass, lead_c, arps, echo_c, rests],  # chorus
        [drums_k, drums_s, drums_h, bass, lead_c, arps, echo_c, rests],  # chorus
        [rests, rests, rests, rests, rests, arps, rests, rests],        # outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
