"""NEON DRIVE - synthwave in A minor, 100 BPM.

Teaching points you can find in the project:
- Song rows are sections (intro, verse, chorus, break, build, outro).
- Track 4 plays whole chords from ONE note: the note is the lowest chord
  tone and CHRD adds the rest (CHRD 0047 = major, 0037 = minor).
- Track 5's first intro note has an instrument number, the rest do not,
  so the FCUT filter sweep keeps opening across four bars.
- Hats use VOLM accents, the build uses a snare crescendo and RTRG rolls.
"""

from lgpt_composer import Phrase, Project, note
from _patterns import (arp_bar, chord_bar, crescendo, octave_bass, rest,
                       voice_progression)

KICK, SNARE, HAT, BASS, PAD, ARP, LEAD, TOM, CRASH, BELL = range(10)

VERSE = ["Am", "F", "C", "G"]
CHORUS = ["F", "G", "Em", "Am"]


def build() -> Project:
    p = Project("NeonDrive", tempo=100)
    p.key("A", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.params["reverb size"] = "200"
    p.params["reverb damp"] = "80"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "110"

    p.synth(KICK, "kick", drive=0x60, volume=0xB8)
    p.synth(SNARE, "snare", decay=0xA0, reverb=0x70, volume=0xA8)
    p.synth(HAT, "hat", volume=0x90)
    p.synth(BASS, "bass", decay=0x98, sustain=0x60, cutoff=0x60, env_amount=0x70,
            drive=0x40, volume=0x7C)
    p.synth(PAD, "pad", attack=0x90, release=0xC0, glide=0x04, reverb=0x90, volume=0x40)
    p.synth(ARP, "pluck", decay=0x98, cutoff=0x48, pan=0x58, delay=0x60, reverb=0x40, volume=0xA8)
    p.synth(LEAD, "lead", delay=0x48, reverb=0x60, volume=0x4C)
    p.synth(TOM, "tom", reverb=0x50, volume=0xA0)
    p.synth(CRASH, "openhat", decay=0xC8, release=0xC0, cutoff=0xB8, reverb=0x70, volume=0x78)
    p.synth(BELL, "bell", reverb=0x90, delay=0x50, volume=0x50)

    # --- drums -----------------------------------------------------------
    kick_half = Phrase.drums("x.......x.x.....", KICK)
    kick_four = Phrase.drums("x...x...x...x...", KICK)
    kick_drop = Phrase.drums("x...x...x.......", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_fill = Phrase.drums("....x.......x.oX", SNARE, accent=0xC8, ghost=0x50)
    hats8 = Phrase.drums("x.x.x.x.x.x.x.x.", HAT)
    hats16 = Phrase.drums("xoXoxoXoxoXoxoXo", HAT, accent=0xC0, ghost=0x48)
    hats_open = Phrase.drums("xoXoxoXoxoXoxoXo", HAT, accent=0xC0, ghost=0x48)

    drums_rest = p.chain([rest()] * 4)
    kick_verse = p.chain([kick_half] * 4)
    kick_chorus = p.chain([kick_four] * 4)
    snare_verse = p.chain([snare, snare, snare, snare_fill])
    hats_verse = p.chain([hats8] * 4)
    hats_chorus = p.chain([hats16, hats16, hats16, hats_open])

    # build: kick keeps pumping, snare rolls up, everything stops for one beat
    build_kick = p.chain([kick_four, kick_four, kick_four, kick_drop])
    roll3 = crescendo("xxxxxxxxxxxxxxxx", SNARE, 0x40, 0x98)
    roll4 = crescendo("xxxxxxxxxxxx....", SNARE, 0xA0, 0xC8)
    for i in range(8, 12):
        roll4.command(i, "RTRG", 0x0003)  # 32nd-note stutter on the last beat
    build_snare = p.chain([snare, Phrase.drums("x.x.x.x.x.x.x.x.", SNARE, normal=0x70), roll3, roll4])

    # toms at the end of the second chorus, crash on the downbeat of choruses
    tom_fill = Phrase.parse(". . . . . . . . . . . . G3 E3 C3 A2", TOM)
    crash = Phrase.drums("x...............", CRASH)
    perc_rest = drums_rest
    perc_chorus_a = p.chain([crash, rest(), rest(), rest()])
    perc_chorus_b = p.chain([rest(), rest(), rest(), tom_fill])

    # --- bass (written 2 octaves above what you hear: preset tune -24) -----
    verse_bass = p.chain([octave_bass("A2", BASS), octave_bass("F2", BASS),
                          octave_bass("C3", BASS), octave_bass("G2", BASS, approach="G#2")])
    verse_bass_to_chorus = p.chain([octave_bass("A2", BASS), octave_bass("F2", BASS),
                                    octave_bass("C3", BASS), octave_bass("G2", BASS)])
    chorus_bass = p.chain([octave_bass("F2", BASS), octave_bass("G2", BASS),
                           octave_bass("E2", BASS), octave_bass("A2", BASS, approach="G2")])
    held = [Phrase.parse("A2 . . . . . . . . . . . . . . .", BASS),
            Phrase.parse("F2 . . . . . . . . . . . . . . .", BASS),
            Phrase.parse("C3 . . . . . . . . . . . . . . .", BASS),
            Phrase.parse("G2 . . . . . . . . . . . . . . .", BASS)]
    intro_bass = p.chain(held)
    build_bass = p.chain([octave_bass("F2", BASS), octave_bass("G2", BASS),
                          octave_bass("E2", BASS), Phrase.parse("E2 . E3 . E2 . E3 . E2 . E3 . - . . .", BASS)])

    # --- chords ------------------------------------------------------------
    verse_v = voice_progression(VERSE, "E3")
    chorus_v = voice_progression(CHORUS, "E3")
    pad_verse = p.chain([chord_bar(v, PAD) for v in verse_v])
    pad_chorus = p.chain([chord_bar(v, PAD) for v in chorus_v])
    pad_outro = p.chain([chord_bar(verse_v[0], PAD, ("VOLM", 0x6000)), Phrase(), Phrase(), rest()])

    arp_v = voice_progression(VERSE, "C3")
    arp_c = voice_progression(CHORUS, "C3")
    # Intro: one instrument number on the first note, then FCUT opens slowly
    intro_arp = []
    for bar, v in enumerate(arp_v):
        ph = arp_bar(v, None, first_instr=ARP if bar == 0 else None)
        if bar == 0:
            ph.command(0, "FCUT", 0x60C0)  # sweep cutoff to C0 over ~4 bars
        intro_arp.append(ph)
    arp_intro = p.chain(intro_arp)
    arp_verse = p.chain([arp_bar(v, ARP) for v in arp_v])
    arp_chorus = p.chain([arp_bar(v, ARP) for v in arp_c])

    # --- melodies ----------------------------------------------------------
    verse_lead = p.chain([
        Phrase.parse("E3 . . . . . . . A3 . . . G3 . E3 .", LEAD),
        Phrase.parse("F3 . . . . . . . . . . . E3 . C3 .", LEAD),
        Phrase.parse("E3 . . . . . . . G3 . . . E3 . D3 .", LEAD),
        Phrase.parse("D3 . . . . . . . . . . . - . . .", LEAD),
    ])
    chorus_a = p.chain([
        Phrase.parse("A3 . . C4 . . E4 . . . . . D4 . C4 .", LEAD),
        Phrase.parse("D4 . . . . . . . B3 . . . G3 . A3 .", LEAD),
        Phrase.parse("B3 . . . . . . . G3 . E3 . . . G3 .", LEAD),
        Phrase.parse("A3 . . . . . . . . . . . - . . .", LEAD),
    ])
    chorus_b = p.chain([
        Phrase.parse("A3 . . C4 . . E4 . . . . . D4 . C4 .", LEAD),
        Phrase.parse("D4 . . E4 . . G4 . . . . . E4 . D4 .", LEAD),
        Phrase.parse("E4 . . . D4 . B3 . . . G3 . . . A3 .", LEAD),
        Phrase.parse("A3 . . . . . . . . . - . . . . .", LEAD),
    ])
    # Break: the chorus hook on a bell, an octave down, sparse
    break_bell = p.chain([
        Phrase.parse("A2 . . C3 . . E3 . . . . . . . . .", BELL),
        Phrase.parse("D3 . . . . . . . B2 . . . . . . .", BELL),
        Phrase.parse("B2 . . . . . . . G2 . E2 . . . . .", BELL),
        Phrase.parse("A2 . . . . . . . . . . . . . . .", BELL),
    ])
    lead_rest = drums_rest

    # --- arrangement: one song row = one 4-bar section ---------------------
    #        kick         snare        hats          bass         pad         arp         lead          perc
    rows = [
        [drums_rest, drums_rest, drums_rest, drums_rest, pad_verse, arp_intro, lead_rest, perc_rest],        # intro
        [kick_verse, drums_rest, hats_verse, intro_bass, pad_verse, arp_verse, lead_rest, perc_rest],        # intro 2
        [kick_verse, snare_verse, hats_verse, verse_bass, pad_verse, arp_verse, lead_rest, perc_rest],       # verse
        [kick_verse, snare_verse, hats_verse, verse_bass_to_chorus, pad_verse, arp_verse, verse_lead, perc_rest],  # verse + lead
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_a, perc_chorus_a],  # chorus
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_b, perc_chorus_b],  # chorus 2
        [drums_rest, drums_rest, drums_rest, intro_bass, pad_verse, arp_verse, break_bell, perc_rest],       # break
        [build_kick, build_snare, hats_verse, build_bass, pad_chorus, arp_chorus, lead_rest, perc_rest],     # build
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_a, perc_chorus_a],  # chorus
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_b, perc_chorus_b],  # chorus 2
        [kick_verse, drums_rest, hats_verse, intro_bass, pad_verse, arp_verse, verse_lead, perc_chorus_a],   # outro
        [drums_rest, drums_rest, drums_rest, drums_rest, pad_outro, arp_intro, lead_rest, perc_rest],        # tail
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
