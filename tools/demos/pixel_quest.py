"""PIXEL QUEST - an 8-bit adventure theme in C major, 140 BPM.

Every sound is the WAV engine (the chip oscillator): pulse leads, a
triangle bass, noise drums and a sine "zap" kick. What it shows:

- TABLES: the arpeggio on track 6 is not ARPG but a table. Table 00 steps
  the pitch 0 / +4 / +7 / +12 / +7 / +4 and loops (HOP), one row every 3
  ticks (TICK in the table itself); table 01 is the same with +3, for minor
  chords. A phrase picks one per bar with TABL.
- ARPG: the title screen and the ending use the classic one-note chord
  (ARPG 0047 major, 0037 minor) on the lead instead.
- The echo track (7) plays the lead's notes one 16th late (DLAY 0003) with a
  delay send, panned the other way: a wide, doubled chip lead.
- VIBR on long lead notes, LEGA slides, a VOLM crescendo + RTRG snare
  build, CHNC ghost hats.
- A key change: TRSP 0002 lifts the last two choruses a whole tone, and the
  song resets it (TRSP 0000) on its first step so the next play starts in C.
- Mixer levels, master EQ and the limiter finish the mix.

Sections: title, groove, verse x2, chorus x2, bridge (A minor, the bell
takes the tune), build, chorus x2 a tone up, stage clear.
"""

from lgpt_composer import Phrase, Project, note
from _patterns import crescendo, rest

KICK, SNARE, HAT, BASS, LEAD, ARP, ECHO, BELL = range(8)

MAJOR_ARP, MINOR_ARP = 0x00, 0x01


def arp_bar(root: str, table: int) -> Phrase:
    """One held note a bar; the table turns it into a fast arpeggio."""
    p = Phrase.parse(f"{root} . . . . . . . . . . . . . . .", ARP)
    p.command(0, "TABL", table)
    return p


def lead(text: str, vibrato: dict[int, int] | None = None, instr: int = LEAD) -> Phrase:
    p = Phrase.parse(text, instr)
    for step, depth in (vibrato or {}).items():
        p.command(step, "VIBR", depth)
    return p


def echo_of(phrases: list[Phrase]) -> list[Phrase]:
    """The same notes on the echo instrument, a 16th late."""
    out = []
    for ph in phrases:
        e = Phrase()
        for i, s in enumerate(ph.steps):
            if s.note != 0xFF:
                e.set(i, note=s.note, instr=ECHO)
                e.command(i, "DLAY", 0x0003)
            elif s.cmd1 == "KILL":
                e.set(i, cmd1="KILL", param1=0)
        out.append(e)
    return out


def build() -> Project:
    p = Project("PixelQuest", tempo=140)
    p.key("C", "Ionian mode (major)")
    p.set("softclip", "Subtle")
    p.set("pregain", 64)
    p.params["reverb size"] = "120"
    p.params["reverb damp"] = "110"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "80"
    p.params["eq low gain"] = "140"      # a little more weight under the chips
    p.params["eq high gain"] = "120"     # tame the pulse fizz
    p.params["limiter drive"] = "24"
    # Mixer: the tune on top, drums just under it, the arp behind
    p.mixer(LEAD, 0xD0)
    p.mixer(ARP, 0x98)
    p.mixer(ECHO, 0xA0)
    p.mixer(BELL, 0xB0)

    # --- the chip kit: WAV engine throughout --------------------------------
    p.synth(KICK, "zap", tune=-26, volume=0xFF)
    p.synth(SNARE, "noise hat", tune=0, decay=0x88, cutoff=0x60, filter="highpass",
            reverb=0x30, volume=0x88, pan=0x7F)
    p.synth(HAT, "noise hat", volume=0x70)
    p.synth(BASS, "chip bass", sustain=0x80, release=0x20, volume=0x88)
    p.synth(LEAD, "chip lead", glide=0x10, volume=0x58)
    p.synth(ARP, "chip lead", wav_shape="pulse12", lfo_amount=0, glide=0, delay=0,
            decay=0x98, sustain=0x70, pan=0x50, volume=0x44)
    p.synth(ECHO, "chip lead", wav_shape="pulse50", delay=0x70, pan=0xB0, volume=0x38)
    p.synth(BELL, "lofi bell", reverb=0x80, delay=0x40, volume=0x70)

    # Arpeggio tables: one row every 3 ticks, looping
    p.table(MAJOR_ARP, [("PTCH", 0x0000, "TICK", 0x0003), ("PTCH", 0x0004), ("PTCH", 0x0007),
                        ("PTCH", 0x000C), ("PTCH", 0x0007), ("PTCH", 0x0004), ("HOP ", 0x0000)])
    p.table(MINOR_ARP, [("PTCH", 0x0000, "TICK", 0x0003), ("PTCH", 0x0003), ("PTCH", 0x0007),
                        ("PTCH", 0x000C), ("PTCH", 0x0007), ("PTCH", 0x0003), ("HOP ", 0x0000)])

    # --- drums --------------------------------------------------------------
    kick_verse = Phrase.drums("x.....x...x.....", KICK)
    kick_chorus = Phrase.drums("x...x...x...x...", KICK)
    kick_fill = Phrase.drums("x...x...x.x.xxxx", KICK)
    kick_half = Phrase.drums("x...............", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_fill = Phrase.drums("....x.......x.xX", SNARE, accent=0xFF, ghost=0x70)
    snare_half = Phrase.drums("........x.......", SNARE)
    # Build: 16ths that swell up, then 32nds (RTRG) on the last beat
    snare_rise = crescendo("xxxxxxxxxxxxxxxx", SNARE, 0x30, 0x90)
    snare_peak = crescendo("xxxxxxxxxxxx....", SNARE, 0x98, 0xD0)
    for i in range(8, 12):
        snare_peak.command(i, "RTRG", 0x0003)
    snare_peak.set(12, cmd1="KILL", param1=0)   # RTRG would run on into the chorus
    hats = Phrase.drums("x.x.x.x.x.x.x.x.", HAT, normal=0x90)
    hats16 = Phrase.drums("xoxoxoxoxoxoxoxo", HAT, ghost=0x48)
    for step in range(1, 16, 2):
        hats16.command(step, "CHNC", 0x00A0)  # ghost hats play most of the time
    rests = p.chain([rest()] * 4)

    kick_v = p.chain([kick_verse, kick_verse, kick_verse, kick_fill])
    kick_c = p.chain([kick_chorus, kick_chorus, kick_chorus, kick_fill])
    kick_b = p.chain([kick_half] * 4)
    kick_up = p.chain([kick_chorus, kick_chorus, kick_chorus, Phrase.drums("x...x...xxxxxxxx", KICK)])
    snare_v = p.chain([snare, snare, snare, snare_fill])
    snare_b = p.chain([snare_half] * 4)
    snare_up = p.chain([snare, snare, snare_rise, snare_peak])
    hats_v = p.chain([hats] * 4)
    hats_c = p.chain([hats16] * 4)

    # --- bass: triangle (sounds two octaves below the notes) ------------------
    # Root on the downbeat, then octave jumps in the gaps between the kicks
    # (verse kicks on 0 6 10, chorus on 0 4 8 12), so the two never smear
    def bass_bar(root: str, walk: str | None = None, chorus: bool = False) -> Phrase:
        low = note(root)
        steps = (0, 2, 3, 6, 7, 10, 11, 14) if chorus else (0, 2, 4, 7, 8, 11, 12, 14)
        ph = Phrase()
        for k, s in enumerate(steps):
            ph.set(s, note=low if k % 2 == 0 else low + 12, instr=BASS)
        if walk:
            ph.set(14, note=note(walk), instr=BASS)
        return ph
    bass_v = p.chain([bass_bar("C3"), bass_bar("G2"), bass_bar("A2"), bass_bar("F2", "G2")])
    bass_c = p.chain([bass_bar("F2", chorus=True), bass_bar("G2", chorus=True), bass_bar("E2", chorus=True),
                     bass_bar("A2", "G2", chorus=True)])
    bass_c_end = p.chain([bass_bar("F2", chorus=True), bass_bar("G2", chorus=True), bass_bar("C3", chorus=True),
                         bass_bar("C3", "G2", chorus=True)])
    bass_b = p.chain([Phrase.parse("A2 . . . . . . . A2 . . . . . . .", BASS),
                      Phrase.parse("F2 . . . . . . . F2 . . . . . . .", BASS),
                      Phrase.parse("G2 . . . . . . . G2 . . . . . . .", BASS),
                      Phrase.parse("E2 . . . . . . . E2 . E3 . E2 . E3 .", BASS)])
    bass_up = p.chain([bass_bar("F2", chorus=True), bass_bar("G2", chorus=True), bass_bar("F2", chorus=True),
                       Phrase.parse("G2 . G3 . G2 . G3 . G2 G2 G2 G2 G2 G2 G2 G2", BASS)])

    # --- arpeggios from tables ---------------------------------------------
    arp_v = p.chain([arp_bar("C4", MAJOR_ARP), arp_bar("G3", MAJOR_ARP),
                     arp_bar("A3", MINOR_ARP), arp_bar("F3", MAJOR_ARP)])
    arp_c = p.chain([arp_bar("F4", MAJOR_ARP), arp_bar("G4", MAJOR_ARP),
                     arp_bar("E4", MINOR_ARP), arp_bar("A4", MINOR_ARP)])
    arp_c_end = p.chain([arp_bar("F4", MAJOR_ARP), arp_bar("G4", MAJOR_ARP),
                         arp_bar("C4", MAJOR_ARP), arp_bar("C4", MAJOR_ARP)])
    arp_b = p.chain([arp_bar("A3", MINOR_ARP), arp_bar("F3", MAJOR_ARP),
                     arp_bar("G3", MAJOR_ARP), arp_bar("E3", MAJOR_ARP)])

    # --- the tune --------------------------------------------------------------
    # Title screen: the fanfare, then ARPG chords on the lead
    title = Phrase.parse("C5 . G4 . C5 . E5 . G5 . . . . . . .", LEAD)
    title.command(0, "TRSP", 0x0000)                      # every play starts in C
    title.command(8, "VIBR", 0x0048)
    title_chord = Phrase.parse("F4 . . . . . . . G4 . . . . . . .", LEAD)
    title_chord.command(0, "ARPG", 0x0047)
    title_chord.command(8, "ARPG", 0x0047)
    title_end = Phrase.parse("C5 . . . . . . . . . . . - . . .", LEAD).command(0, "ARPG", 0x0047)
    lead_title = p.chain([title, title_chord, title, title_end])

    verse = [lead("E5 . D5 . C5 . . . G4 . . . C5 . E5 .", {4: 0x0046}),
             lead("D5 . . . . . B4 . G4 . . . B4 . D5 .", {0: 0x0046}),
             lead("C5 . B4 . A4 . . . E4 . . . A4 . C5 .", {4: 0x0046}),
             lead("C5 . . . A4 . F4 . A4 . . . G4 . . .", {12: 0x0048})]
    verse2 = verse[:3] + [lead("C5 . . . A4 . C5 . D5 . . . . . - .", {8: 0x0048})]
    chorus = [lead("A5 . . . G5 . F5 . G5 . . . A5 . C6 .", {12: 0x0048}),
              lead("B5 . . . . . G5 . D5 . . . G5 . B5 .", {0: 0x0048}),
              lead("G5 . . . E5 . . . B4 . . . E5 . G5 .", {12: 0x0048}),
              lead("A5 . . . . . . . . . . . - . . .", {0: 0x0058})]
    chorus_end = chorus[:2] + [lead("C6 . . . . . G5 . E5 . C5 . . . . .", {0: 0x0058}),
                               lead("E5 . D5 . C5 . . . . . . . - . . .", {4: 0x0058})]
    chorus[0].command(0, "LEGA", 0x08F4)                  # swoop up into the chorus
    lead_v = p.chain(verse)
    lead_v2 = p.chain(verse2)
    lead_c = p.chain(chorus)
    lead_c_end = p.chain(chorus_end)
    echo_c = p.chain(echo_of(chorus))
    echo_c_end = p.chain(echo_of(chorus_end))
    echo_v2 = p.chain(echo_of(verse2))

    # Key change: the last choruses a whole tone up
    key_up = [c.copy() for c in chorus]
    key_up[0].command(4, "TRSP", 0x0002)
    lead_key = p.chain(key_up)

    # Bridge: the bell sings over A minor
    bell_b = p.chain([Phrase.parse("E5 . . . A5 . . . C6 . . . B5 . A5 .", BELL),
                      Phrase.parse("A5 . . . . . . . F5 . . . . . . .", BELL),
                      Phrase.parse("G5 . . . B5 . . . D6 . . . C6 . B5 .", BELL),
                      Phrase.parse("G#5 . . . . . . . B5 . . . . . . .", BELL)])

    # Stage clear: rising jingle, one ARPG chord, done
    clear = p.chain([
        Phrase.parse("C5 . E5 . G5 . C6 . . . G5 . C6 . . .", LEAD),
        Phrase.parse("F5 . . . . . A5 . . . . . G5 . . .", LEAD).command(0, "ARPG", 0x0047),
        Phrase.parse("C6 . . . . . . . . . . . . . . .", LEAD).command(0, "ARPG", 0x0047),
        Phrase().set(0, cmd1="KILL", param1=0),
    ])
    clear_bass = p.chain([bass_bar("C3", chorus=True), bass_bar("F2", chorus=True),
                          Phrase.parse("C3 . . . . . . . . . . . . . - .", BASS), rest()])
    clear_kick = p.chain([kick_chorus, kick_chorus, kick_half, rest()])

    rows = [
        # kick    snare     hats    bass       lead        arp        echo       bell
        [rests, rests, rests, rests, lead_title, arp_v, rests, rests],              # 00 title
        [kick_v, snare_v, hats_v, bass_v, rests, arp_v, rests, rests],              # 01 groove
        [kick_v, snare_v, hats_v, bass_v, lead_v, arp_v, rests, rests],             # 02 verse
        [kick_v, snare_v, hats_v, bass_v, lead_v2, arp_v, echo_v2, rests],          # 03 verse 2
        [kick_c, snare_v, hats_c, bass_c, lead_c, arp_c, echo_c, rests],            # 04 chorus
        [kick_c, snare_v, hats_c, bass_c_end, lead_c_end, arp_c_end, echo_c_end, rests],  # 05 chorus 2
        [kick_b, snare_b, hats_v, bass_b, rests, arp_b, rests, bell_b],             # 06 bridge
        [kick_up, snare_up, hats_c, bass_up, rests, arp_c, rests, bell_b],          # 07 build
        [kick_c, snare_v, hats_c, bass_c, lead_key, arp_c, echo_c, rests],          # 08 chorus, a tone up
        [kick_c, snare_v, hats_c, bass_c_end, lead_c_end, arp_c_end, echo_c_end, rests],  # 09
        [clear_kick, rests, hats_v, clear_bass, clear, rests, rests, rests],        # 0A stage clear
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
