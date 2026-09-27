"""NEON DRIVE - synthwave in A minor, 100 BPM.

A night drive: 80s drum machine with a huge snare, a pumping octave bass,
a glassy arpeggio and a singing lead. What it shows:

- The DRUM engine: 909 kick and snare, 808 hats (tracks 1-3). The snare's
  big reverb send is the 80s "gated" sound.
- Sidechain pump: the bass (track 4) has a `trig` mod slot fired by the
  kick on track 1 that ducks its volume, so the octave 8ths never smear the
  kick even though they play on it.
- Track 5 plays whole chords from ONE note: the note is the
  lowest chord tone and CHRD adds the rest (CHRD 0047 = major, 0037 = minor).
- Track 6's first intro note has an instrument number, the rest do not, so
  the FCUT filter sweep keeps opening across four bars.
- Verses keep the kick on 1 and 3 and the arp low; choruses go four on the
  floor with open hats and the lead an octave up, so the chorus lifts.
- Hats use VOLM accents, the build uses a snare crescendo and RTRG rolls,
  the lead bends into notes with LEGA and sings with VIBR.

The mix is set with instrument volumes and sends only: the mixer, master EQ
and limiter stay at their defaults (the mixer-levels, eq-screen and
master-limiter sim tests start from them).
"""

from lgpt_composer import Phrase, Project
from _patterns import (arp_bar, chord_bar, crescendo, octave_bass, rest,
                       voice_progression)

KICK, SNARE, HAT, BASS, PAD, ARP, LEAD, TOM, CRASH, BELL, OPEN = range(11)

VERSE = ["Am", "F", "C", "G"]
CHORUS = ["F", "G", "Em", "Am"]


def build() -> Project:
    p = Project("NeonDrive", tempo=100)
    p.key("A", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.set("pregain", 64)
    p.params["reverb size"] = "210"
    p.params["reverb damp"] = "90"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "100"

    # --- drums: the DRUM engine ------------------------------------------
    p.synth(KICK, "909 kick", tune=-26, drum_decay=0x90, volume=0xB0)
    p.synth(SNARE, "909 snare", reverb=0xA8, volume=0xF0)
    p.synth(HAT, "808 hat", volume=0x70)
    p.synth(OPEN, "808 open", volume=0x60)
    p.synth(TOM, "808 tom", reverb=0x60, volume=0xB0)
    p.synth(CRASH, "openhat", decay=0xC8, release=0xC0, cutoff=0xB8, reverb=0x70, volume=0x70)

    # --- bass: octave 8ths, ducked by the kick (sidechain) -----------------
    p.synth(BASS, "bass", decay=0x98, sustain=0x60, cutoff=0x58, env_amount=0x78,
            drive=0x40, volume=0x80,
            mod1_type="trig", mod1_dest="volume", mod1_amount=-90,
            mod1_p1=0x00, mod1_p2=0x10, mod1_p3=0x98, mod1_p4=0)

    # --- chords and lines --------------------------------------------------
    # The pad sits above the bass: voiced around A3, its own EQ cuts the lows
    p.synth(PAD, "pad", attack=0x90, release=0xC0, glide=0x04, reverb=0x98, volume=0x48,
            eq_low_gain=0x40)
    p.synth(ARP, "pluck", decay=0x98, cutoff=0x50, pan=0x50, delay=0x68, reverb=0x40, volume=0x78)
    p.synth(LEAD, "lead", delay=0x58, reverb=0x60, glide=0x10, volume=0x58)
    p.synth(BELL, "bell", reverb=0x98, delay=0x60, pan=0xA8, volume=0x50)

    # --- drums -------------------------------------------------------------
    kick_half = Phrase.drums("x.......x.x.....", KICK)
    kick_four = Phrase.drums("x...x...x...x...", KICK)
    kick_drop = Phrase.drums("x...x...x.......", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_fill = Phrase.drums("....x.......x.oX", SNARE, accent=0xE0, ghost=0x60)
    hats8 = Phrase.drums("x.x.x.x.x.x.x.x.", HAT, normal=0xC0)
    hats16 = Phrase.drums("xoXoxoXoxoXoxoXo", HAT, accent=0xE0, ghost=0x50)
    # Open hat on the offbeats (its own instrument, same track as the hats)
    hats_open = Phrase.merge(Phrase.drums("xo.oxo.oxo.oxo.o", HAT, ghost=0x50),
                             Phrase.drums("..x...x...x...x.", OPEN))

    drums_rest = p.chain([rest()] * 4)
    kick_verse = p.chain([kick_half] * 4)
    kick_chorus = p.chain([kick_four] * 4)
    snare_verse = p.chain([snare, snare, snare, snare_fill])
    hats_verse = p.chain([hats8] * 4)
    hats_chorus = p.chain([hats_open, hats_open, hats_open, hats16])

    # Build: the kick pumps, the snare rolls up, everything stops for one beat
    build_kick = p.chain([kick_four, kick_four, kick_four, kick_drop])
    roll3 = crescendo("xxxxxxxxxxxxxxxx", SNARE, 0x40, 0xA0)
    roll4 = crescendo("xxxxxxxxxxxx....", SNARE, 0xA8, 0xE0)
    for i in range(8, 12):
        roll4.command(i, "RTRG", 0x0003)  # 32nd-note stutter on the last beat
    roll4.set(12, cmd1="KILL", param1=0)  # ...and silence: RTRG would run on into the drop
    build_snare = p.chain([snare, Phrase.drums("x.x.x.x.x.x.x.x.", SNARE, normal=0x70), roll3, roll4])

    # Toms into the second chorus, a crash on each chorus downbeat
    tom_fill = Phrase.parse(". . . . . . . . . . . . G3 E3 C3 A2", TOM)
    crash = Phrase.drums("x...............", CRASH)
    perc_rest = drums_rest
    perc_chorus_a = p.chain([crash, rest(), rest(), rest()])
    perc_chorus_b = p.chain([rest(), rest(), rest(), tom_fill])

    # --- bass (written 2 octaves above what you hear: preset tune -24) -----
    verse_bass = p.chain([octave_bass("A2", BASS), octave_bass("F2", BASS),
                          octave_bass("C3", BASS), octave_bass("G2", BASS, approach="G#2")])
    chorus_bass = p.chain([octave_bass("F2", BASS), octave_bass("G2", BASS),
                           octave_bass("E2", BASS), octave_bass("A2", BASS, approach="G2")])
    held = [Phrase.parse("A2 . . . . . . . . . . . . . - .", BASS),
            Phrase.parse("F2 . . . . . . . . . . . . . - .", BASS),
            Phrase.parse("C3 . . . . . . . . . . . . . - .", BASS),
            Phrase.parse("G2 . . . . . . . . . . . . . - .", BASS)]
    intro_bass = p.chain(held)
    build_bass = p.chain([octave_bass("F2", BASS), octave_bass("G2", BASS),
                          octave_bass("E2", BASS),
                          Phrase.parse("E2 . E3 . E2 . E3 . E2 . E3 . - . . .", BASS)])

    # --- chords --------------------------------------------------------------
    verse_v = voice_progression(VERSE, "A3")
    chorus_v = voice_progression(CHORUS, "A3")
    pad_verse = p.chain([chord_bar(v, PAD) for v in verse_v])
    pad_chorus = p.chain([chord_bar(v, PAD) for v in chorus_v])
    pad_outro = p.chain([chord_bar(verse_v[0], PAD, ("VOLM", 0x6000)), Phrase(), Phrase(), rest()])

    # The arp lives above the pad, between it and the lead
    arp_v = voice_progression(VERSE, "E4")
    arp_c = voice_progression(CHORUS, "E4")
    # Intro: one instrument number on the first note, then FCUT opens slowly
    intro_arp = []
    for bar, v in enumerate(arp_v):
        ph = arp_bar(v, None, first_instr=ARP if bar == 0 else None)
        if bar == 0:
            ph.command(0, "FCUT", 0x60C0)  # sweep cutoff to C0 over ~4 bars
        intro_arp.append(ph)
    arp_intro = p.chain(intro_arp)
    # Verse: every other 16th, quieter, so the lead has room
    arp_verse = p.chain([arp_bar(v, ARP, step_every=2) for v in arp_v])
    arp_chorus = p.chain([arp_bar(v, ARP) for v in arp_c])

    # --- melodies --------------------------------------------------------------
    # Verse: low and breathy; LEGA slides into the long notes
    v1 = Phrase.parse("E4 . . . . . . . A4 . . . G4 . E4 .", LEAD).command(0, "LEGA", 0x10FE)
    v2 = Phrase.parse("F4 . . . . . . . . . . . E4 . C4 .", LEAD).command(0, "VIBR", 0x0046)
    v3 = Phrase.parse("E4 . . . . . . . G4 . . . E4 . D4 .", LEAD).command(0, "LEGA", 0x10FE)
    v4 = Phrase.parse("D4 . . . . . . . . . . . - . . .", LEAD).command(0, "VIBR", 0x0046)
    verse_lead = p.chain([v1, v2, v3, v4])
    # Chorus: an octave up, the hook climbs A-C-E and falls back
    c1 = Phrase.parse("A4 . . C5 . . E5 . . . . . D5 . C5 .", LEAD).command(6, "VIBR", 0x0048)
    c2 = Phrase.parse("D5 . . . . . . . B4 . . . G4 . A4 .", LEAD).command(0, "VIBR", 0x0048)
    c3 = Phrase.parse("B4 . . . . . . . G4 . E4 . . . G4 .", LEAD)
    c4 = Phrase.parse("A4 . . . . . . . . . . . - . . .", LEAD).command(0, "VIBR", 0x0058)
    chorus_a = p.chain([c1, c2, c3, c4])
    d2 = Phrase.parse("D5 . . E5 . . G5 . . . . . E5 . D5 .", LEAD).command(6, "VIBR", 0x0048)
    d3 = Phrase.parse("E5 . . . D5 . B4 . . . G4 . . . A4 .", LEAD)
    d4 = Phrase.parse("A4 . . . . . . . . . - . . . . .", LEAD).command(0, "LEGA", 0x10FE)
    chorus_b = p.chain([c1, d2, d3, d4])
    # Break: the chorus hook on a bell, sparse, echoing
    break_bell = p.chain([
        Phrase.parse("A4 . . C5 . . E5 . . . . . . . . .", BELL),
        Phrase.parse("D5 . . . . . . . B4 . . . . . . .", BELL),
        Phrase.parse("B4 . . . . . . . G4 . E4 . . . . .", BELL),
        Phrase.parse("A4 . . . . . . . . . . . . . . .", BELL),
    ])
    # Final chorus: the bell doubles the hook an octave up, far right
    bell_double = p.chain([
        Phrase.parse("A5 . . C6 . . E6 . . . . . D6 . C6 .", BELL),
        Phrase.parse("D6 . . . . . . . B5 . . . G5 . A5 .", BELL),
        Phrase.parse("B5 . . . . . . . G5 . E5 . . . G5 .", BELL),
        Phrase.parse("A5 . . . . . . . . . . . - . . .", BELL),
    ])
    lead_rest = drums_rest

    # --- arrangement: one song row = one 4-bar section ---------------------
    #        kick         snare        hats          bass         pad         arp         lead          perc
    rows = [
        [drums_rest, drums_rest, drums_rest, drums_rest, pad_verse, arp_intro, lead_rest, perc_rest],       # intro
        [kick_verse, drums_rest, hats_verse, intro_bass, pad_verse, arp_intro, lead_rest, perc_rest],       # intro 2
        [kick_verse, snare_verse, hats_verse, verse_bass, pad_verse, drums_rest, verse_lead, perc_rest],    # verse
        [kick_verse, snare_verse, hats_verse, verse_bass, pad_verse, arp_verse, verse_lead, perc_rest],     # verse 2
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_a, perc_chorus_a],  # chorus
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_b, perc_chorus_b],  # chorus 2
        [drums_rest, drums_rest, drums_rest, intro_bass, pad_verse, arp_verse, lead_rest, break_bell],      # break
        [build_kick, build_snare, hats_verse, build_bass, pad_chorus, arp_chorus, lead_rest, perc_rest],    # build
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_a, bell_double],  # chorus
        [kick_chorus, snare_verse, hats_chorus, chorus_bass, pad_chorus, arp_chorus, chorus_b, perc_chorus_b],  # chorus 2
        [kick_verse, drums_rest, hats_verse, intro_bass, pad_verse, arp_verse, verse_lead, perc_chorus_a],  # outro
        [drums_rest, drums_rest, drums_rest, drums_rest, pad_outro, arp_intro, lead_rest, perc_rest],       # tail
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
