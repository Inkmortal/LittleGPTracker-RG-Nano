"""JOYRIDE - funk / city pop with a video-game soundtrack feel, C major, 112 BPM.

A feature tour you can dance to: every kind of sound the app makes, the
effects and the mix, in a song that stays fun the whole way through.

    track 1  KICK   drum engine "909 kick", syncopated funk patterns; the
                    808 toms take the fills at the end of each chorus
    track 2  SNARE  drum engine "909 snare" with ghost notes (VOLM), a
                    ROLL that swells into every chorus, reverb send
    track 3  HATS   drum engine "808 hat" 16ths with accents; the ghosts
                    play by chance (CHNC); an "808 open" hat on the 4th pass
                    only (NTH), choked by the next closed hat
    track 4  BASS   FM4 "slap bass": a funk line with octave pops and ghost
                    notes, one bar that the chains transpose
    track 5  PAD    HyperSynth "strings", chord maj7 with scale on: one note
                    a bar becomes Cmaj7, Am7, Dm7, G7... In the break a
                    table gates it (TABL + TICK)
    track 6  KEYS   FM4 "epiano" comping, CHRD voicings that move smoothly
    track 7  LEAD   wav "chip lead" (pulse wave, vibrato, glide, echo): the
                    hook, with LEGA slides, PTCH bends and VIBR; a phys
                    "vibes" plays it slowly in the break
    track 8  COLOR  phys "kalimba" arpeggios with echo, a trumpet sample
                    (brass pack) for the horn stabs, a riser sample (textures)
                    into every chorus

Two progressions from one bar each: the verse I-vi-ii-V (Cmaj7 Am7 Dm7 G7)
and the chorus "royal road" IV-V-iii-vi (Fmaj7 G7 Em7 Am7), the city pop
classic. The last choruses go up a whole tone to D: the chains transpose
the parts by 2 more, and SCAL moves the song's key so the HyperSynth
chords follow.

Mix: sends to the shared reverb, echo (dotted 8ths) and chorus; the pad's
own EQ keeps it out of the low end; the master EQ, the limiter and the
Project Drive give the whole song its level.
"""

from __future__ import annotations

from lgpt_composer import NO_INSTR, Phrase, Project, chrd_param, note
from _kit import use
from _patterns import rest, voice_progression

(KICK, SNARE, HAT, OPEN, TOM, BASS, PAD, KEYS, LEAD, KALIMBA, BRASS, RISER, VIBES) = range(13)

VERSE = ["Cmaj7", "Am7", "Dm7", "G7"]
CHORUS = ["Fmaj7", "G7", "Em7", "Am7"]
# Bass and pad: one bar rooted on C, transposed per bar by the chain
VERSE_T = [0x00, 0x09, 0x02, 0x07]
CHORUS_T = [0x05, 0x07, 0x04, 0x09]
UP = 2                     # the key change: everything a whole tone up
SCAL_C = 0x0015            # key C (00), scale 15 = Ionian (major)
SCAL_D = 0x0215            # key D


def up(ts: list[int], by: int = UP) -> list[int]:
    return [(t + by) & 0xFF for t in ts]


# --- drums ------------------------------------------------------------------

def hats(open_hat: bool, level: int = 0xC0) -> Phrase:
    """Accented 16ths; the in-between ghosts play most of the time; an open
    hat on step E, only on the 4th pass of every 4 (NTH), cut short by the
    next closed hat like a real hi-hat. The verse plays them softer."""
    ph = Phrase()
    for i in range(16):
        if i % 4 == 0:
            ph.set(i, note=note("C3"), instr=HAT).command(i, "VOLM", level)
        elif i % 2 == 0:
            ph.set(i, note=note("C3"), instr=HAT).command(i, "VOLM", level * 2 // 3)
        else:
            ph.set(i, note=note("C3"), instr=HAT).command(i, "VOLM", level * 3 // 8)
            ph.command(i, "CHNC", 0x00B0)
    if open_hat:
        ph.set(14, note=note("C3"), instr=OPEN, cmd1="VOLM", param1=0xA0, cmd2="NTH ", param2=0x0044)
    return ph


def tom_fill(kick: str) -> Phrase:
    """The kick pattern for the first half, then four toms walking down."""
    ph = Phrase.drums(kick[:10] + "......", KICK)
    for step, n in ((10, "G3"), (12, "E3"), (13, "D3"), (14, "C3"), (15, "A2")):
        ph.set(step, note=note(n), instr=TOM)
    return ph


KICK_VERSE = "x......x..x....."
KICK_CHORUS = "x.....x...x..x.."
KICK_FOUR = "x...x...x...x..."


def snare_roll_bar() -> Phrase:
    """Backbeat on 2, then a roll from beat 4 that swells into the chorus."""
    ph = Phrase.drums("....X..o.o......", SNARE, accent=0xD0, ghost=0x38)
    ph.set(12, note=note("C3"), instr=SNARE).command(12, "VOLM", 0x30)
    ph.command(12, "ROLL", 0x0092)        # a hit every 2 ticks, 8 louder each
    return ph


# --- harmony ----------------------------------------------------------------

def bass_bar(line: str) -> Phrase:
    return Phrase.parse(line, BASS)


# Written two octaves up (the preset tunes down 24): C3 sounds as a low C
BASS_VERSE = "C3 . - C4 - G3 . C3 . C3:40 C4 - G3 . C4 G3:70"
BASS_CHORUS = "C3 . C4:70 C3 - C3 C4 - C3 . C3:40 C4 - G3 C4 ."
BASS_HELD = "C3 . . . . . . . . . . . . . . ."
BASS_EIGHTS = "C3 . C3:90 . C3 . C3:90 . C3 . C3:90 . C3 . C4 ."


def pad_bar(cmds: tuple[tuple[str, int], ...] = ()) -> Phrase:
    ph = Phrase.parse("C4 " + ". " * 15, PAD)     # up high, out of the bass's way
    for cmd, param in cmds:
        ph.command(0, cmd, param)
    return ph


def keys_bar(voicing: list[int], shift: int = 0, level: int = 0xB0) -> Phrase:
    """City pop comping: a chord on 1, a softer one on the 'and' of 2, a
    push on the 'e' of 3, cut short before beat 4."""
    root = voicing[0] + shift
    chrd = chrd_param(voicing)
    ph = Phrase()
    for step, vol in ((0, level), (6, level * 3 // 4), (10, level * 7 // 8)):
        ph.set(step, note=root, instr=KEYS).command(step, "VOLM", vol).command(step, "CHRD", chrd)
    ph.set(13, cmd1="KILL", param1=0)
    return ph


def kalimba_bar(voicing: list[int], shift: int = 0, sparse: bool = False) -> Phrase:
    """Chord tones up and down in 16ths (8ths when sparse), an octave up."""
    tones = [n + 12 + shift for n in voicing] + [voicing[0] + 24 + shift]
    order = [0, 1, 2, 3, 4, 3, 2, 1, 0, 2, 4, 2, 1, 3, 4, 3]
    ph = Phrase()
    for i in range(0, 16, 2 if sparse else 1):
        vol = 0xA0 if i % 4 == 0 else 0x68
        ph.set(i, note=tones[order[i] % len(tones)], instr=KALIMBA).command(i, "VOLM", vol)
    return ph


def brass_bar(voicing: list[int], hits: list[tuple[int, int]], shift: int = 0) -> Phrase:
    """Horn-section stabs: a triad (CHRD) on each hit, each cut 2 steps
    later so they stay short and punchy."""
    root = voicing[0] + shift
    chrd = chrd_param(voicing)
    ph = Phrase()
    for step, vol in hits:
        ph.set(step, note=root, instr=BRASS).command(step, "VOLM", vol).command(step, "CHRD", chrd)
        if step + 2 < 16 and all(step + 2 != s for s, _ in hits):
            ph.set(step + 2, cmd1="KILL", param1=0)
    return ph


def build() -> Project:
    p = Project("Joyride", tempo=112)
    p.key("C", "Ionian mode (major)")
    p.groove(0, [7, 5])                        # a light 16th swing
    p.set("pregain", 62)                       # headroom for 8 tracks
    p.params["reverb size"] = str(0xA0)
    p.params["delay steps"] = "3"              # dotted 8ths
    p.params["delay feedback"] = str(0x60)
    p.params["chorus depth"] = str(0x90)
    p.params["eq low gain"] = str(0x90)        # a little weight
    p.params["eq high gain"] = str(0x98)       # and sparkle
    p.params["limiter drive"] = str(0x20)
    p.mixer(5, 0xA8)                           # keys under the lead
    p.mixer(7, 0xE0)                           # horns and kalimba a little forward

    # --- sounds -------------------------------------------------------------
    p.synth(KICK, "808 kick", drum_decay=0x68, volume=0xE0)   # deep but tight
    p.synth(SNARE, "909 snare", reverb=0x48)
    p.synth(HAT, "808 hat")
    p.synth(OPEN, "808 open")
    p.synth(TOM, "808 tom", reverb=0x50)
    p.synth(BASS, "slap bass")
    p.synth(PAD, "strings", hyper_chord="maj7", hyper_scale=True, hyper_sub=0,
            reverb=0x70, chorus=0x40, eq_low_gain=0x40, volume=0x78)
    p.synth(KEYS, "epiano", reverb=0x38)
    p.synth(LEAD, "chip lead", delay=0x50, reverb=0x30)
    p.synth(KALIMBA, "kalimba", delay=0x60, reverb=0x50)
    p.synth(VIBES, "vibes", delay=0x40)
    use(p, BRASS, "brass/trumpet", 0xE0, reverb=0x38)
    use(p, RISER, "textures/riser", 0x90, reverb=0x40)

    # The break's pad gate: full, nearly off, one row per step (VOLM sets the
    # volume outright, so "full" is the pad's own volume, 78)
    p.table(0, [("VOLM", 0x0078) if i % 2 == 0 else ("VOLM", 0x0014) for i in range(16)])

    # --- drums --------------------------------------------------------------
    kick_verse = Phrase.drums(KICK_VERSE, KICK)
    kick_chorus = Phrase.drums(KICK_CHORUS, KICK)
    kick_four = Phrase.drums(KICK_FOUR, KICK)
    kick_half_out = Phrase.drums("x......x........", KICK)   # stops for the roll
    snare_verse = Phrase.drums("....X..o.o..X..o", SNARE, accent=0xD0, ghost=0x38)
    snare_chorus = Phrase.drums("....X..o....X.oo", SNARE, accent=0xD8, ghost=0x40)
    snare_build = Phrase.drums("....X.......X...", SNARE, accent=0xB0)
    hats_verse = hats(open_hat=False, level=0x98)
    hats_chorus = hats(open_hat=True)
    hats_break = Phrase.drums("x...x...x...x...", HAT, normal=0x60)
    # a tom run into the first bar of the song
    count_in = Phrase()
    for step, n in ((8, "G3"), (10, "E3"), (12, "C3"), (14, "A2")):
        count_in.set(step, note=note(n), instr=TOM)

    R = p.chain([rest()] * 4)
    K_intro = p.chain([rest(), rest(), rest(), count_in])
    K_verse = p.chain([kick_verse] * 4)
    K_pre = p.chain([kick_verse, kick_verse, kick_verse, kick_half_out])
    K_chorus = p.chain([kick_chorus, kick_chorus, kick_chorus, tom_fill(KICK_CHORUS)])
    K_build = p.chain([kick_four, kick_four, kick_four, kick_half_out])
    S_verse = p.chain([snare_verse] * 4)
    S_pre = p.chain([snare_verse, snare_verse, snare_verse, snare_roll_bar()])
    S_chorus = p.chain([snare_chorus] * 4)
    S_build = p.chain([snare_build, snare_build, snare_build, snare_roll_bar()])
    H_intro = p.chain([hats_break] * 4)
    H_verse = p.chain([hats_verse] * 4)
    H_build = p.chain([hats_break, hats_break, hats_verse, hats_verse])
    H_chorus = p.chain([hats_chorus] * 4)

    # --- bass: one bar, transposed by the chains ---------------------------
    b_verse, b_chorus = bass_bar(BASS_VERSE), bass_bar(BASS_CHORUS)
    b_held, b_eights = bass_bar(BASS_HELD), bass_bar(BASS_EIGHTS)
    B_verse = p.chain([b_verse] * 4, VERSE_T)
    B_chorus = p.chain([b_chorus] * 4, CHORUS_T)
    B_break = p.chain([b_held] * 4, CHORUS_T)
    B_build = p.chain([b_eights] * 4, CHORUS_T)
    B_chorus_up = p.chain([b_chorus] * 4, up(CHORUS_T))
    B_outro = p.chain([b_verse, b_verse, b_verse, b_held], up(VERSE_T))

    # --- pad: one note a bar, the HyperSynth plays the whole chord ----------
    pad = pad_bar()
    pad_home = pad_bar(cmds=(("SCAL", SCAL_C),))     # the song starts in C
    pad_key_up = pad_bar(cmds=(("SCAL", SCAL_D),))   # the last choruses in D
    pad_gate = pad_bar(cmds=(("TABL", 0x0000), ("TICK", 0x0006)))
    pad_fade = pad_bar(cmds=(("VOLM", 0x6000),))
    P_intro = p.chain([pad_home, pad, pad, pad], VERSE_T)
    P_verse = p.chain([pad] * 4, VERSE_T)
    P_chorus = p.chain([pad] * 4, CHORUS_T)
    P_break = p.chain([pad_gate] * 4, CHORUS_T)
    P_chorus_up = p.chain([pad_key_up, pad, pad, pad], up(CHORUS_T))
    P_outro = p.chain([pad, pad, pad, pad_fade], up(VERSE_T))
    P_tail = p.chain([pad_fade, Phrase(), Phrase(), rest()], up([0x00, 0, 0, 0]))

    # --- keys: voiced for smooth movement ------------------------------------
    verse_v = voice_progression(VERSE, "E4")
    chorus_v = voice_progression(CHORUS, "E4")
    E_verse = p.chain([keys_bar(v, level=0x90) for v in verse_v])
    E_chorus = p.chain([keys_bar(v) for v in chorus_v])
    E_chorus_up = p.chain([keys_bar(v, UP) for v in chorus_v])
    E_outro = p.chain([keys_bar(v, UP, level=0x90) for v in verse_v])

    # --- lead ---------------------------------------------------------------
    verse_a = [
        "E4 . D4 . C4 . . D4 . . E4 . . . . .",
        "C4 . . . . . . . . . A3 . C4 . E4 .",
        "F4 . E4 . D4 . . C4 . . D4 . . . . .",
        "B3 . . . . . . . G3 . A3 . B3 . D4 .",
    ]
    verse_b = [
        "E4 . D4 . C4 . . D4 . . E4 . G4 . . .",
        "A4 . . . G4 . E4 . . . C4 . . . . .",
        "F4 . E4 . D4 . . F4 . . A4 . . . G4 .",
        "G4 . . . . . . . - . . . . . . .",
    ]
    hook_a = [
        "E4 . . D4 . . C4 . . . D4 . E4 . G4 .",
        "F4 . . E4 . . D4 . . . B3 . D4 . . .",
        "E4 . . D4 . . B3 . . . G3 . B3 . D4 .",
        "C4 . . . . . A3 . . . . . - . . .",
    ]
    hook_b = [
        "E4 . . D4 . . C4 . . . D4 . E4 . A4 .",
        "G4 . . F4 . . E4 . . . D4 . B3 . D4 .",
        "E4 . . . D4 . B3 . . . G3 . A3 . B3 .",
        "C4 . . . . . . . B3 . . . A3 . . .",
    ]

    def lead(bars: list[str], flourish: bool, level: int | None = None) -> list[Phrase]:
        out = [Phrase.parse(b, LEAD) for b in bars]
        if level is not None:
            # softer than the preset's own 48: the verse leaves room for the hook
            for ph in out:
                for i, st in enumerate(ph.steps):
                    if st.note != 0xFF:
                        ph.command(i, "VOLM", level)
        # The first note slides in from below; the long notes sing
        out[0].command(0, "LEGA", 0x10FB)
        held = out[3]
        first = next(i for i, s in enumerate(held.steps) if s.note != 0xFF)
        held.command(first, "VIBR", 0x0048)
        if flourish:
            # a bend up to the next note at the end of bar 2
            out[1].command(12, "PTCH", 0x0802)
        return out

    L_verse = p.chain(lead(verse_a, False, 0x34))
    L_verse2 = p.chain(lead(verse_b, True, 0x34))
    L_hook_a = p.chain(lead(hook_a, True))
    L_hook_b = p.chain(lead(hook_b, True))
    L_hook_a_up = p.chain(lead(hook_a, True), [UP] * 4)
    L_hook_b_up = p.chain(lead(hook_b, True), [UP] * 4)
    # Break: the hook, half as busy, on vibes an octave down
    break_melody = [
        "E3 . . . . . C3 . . . . . G3 . . .",
        "F3 . . . . . D3 . . . . . B2 . . .",
        "E3 . . . . . B2 . . . . . G2 . . .",
        "A2 . . . . . . . . . . . . . . .",
    ]
    L_break = p.chain([Phrase.parse(b, VIBES) for b in break_melody])
    # The build: the lead climbs with a chiptune arpeggio (ARPG) on one note
    climb = []
    for n, arp in (("C4", 0x0047), ("D4", 0x0047), ("E4", 0x0037), ("G4", 0x0047)):
        ph = Phrase.parse(f"{n} . . . . . . . . . . . . . . .", LEAD)
        ph.command(0, "ARPG", arp)
        ph.command(0, "VOLM", 0x20 + 0x08 * len(climb))   # growing to the preset's 48
        climb.append(ph)
    L_build = p.chain(climb)

    # --- colour: kalimba, horns, riser ---------------------------------------
    X_intro = p.chain([kalimba_bar(v) for v in verse_v])
    X_verse = p.chain([kalimba_bar(v, sparse=True) for v in verse_v])
    riser_bar = Phrase().set(10, note=note("C3"), instr=RISER)   # peaks on the downbeat
    X_pre = p.chain([kalimba_bar(verse_v[0], sparse=True), kalimba_bar(verse_v[1], sparse=True),
                     riser_bar, Phrase()])
    horn_hits = [
        [(0, 0xFF)],
        [(10, 0xC0), (12, 0xFF)],
        [(0, 0xE0)],
        [(6, 0xB0), (10, 0xC8), (14, 0xFF)],
    ]
    brass_v = voice_progression(["F", "G", "Em", "Am"], "A3")   # triads
    X_chorus = p.chain([brass_bar(v, h) for v, h in zip(brass_v, horn_hits)])
    X_chorus_up = p.chain([brass_bar(v, h, UP) for v, h in zip(brass_v, horn_hits)])
    X_break = p.chain([kalimba_bar(v, sparse=True) for v in chorus_v])
    X_build = p.chain([kalimba_bar(chorus_v[0]), kalimba_bar(chorus_v[1]), riser_bar, Phrase()])
    X_outro = p.chain([kalimba_bar(v, UP) for v in verse_v])
    X_tail = p.chain([kalimba_bar(verse_v[0], UP, sparse=True), Phrase(), Phrase(), rest()])

    # --- arrangement: one song row = one 4-bar section -----------------------
    #          kick      snare     hats       bass         pad          keys         lead         colour
    rows = [
        [K_intro, R, H_intro, R, P_intro, R, R, X_intro],                              # 00 title screen
        [K_verse, S_verse, H_verse, B_verse, P_verse, E_verse, R, X_verse],            # 01 groove
        [K_verse, S_verse, H_verse, B_verse, R, E_verse, L_verse, R],                  # 02 verse
        [K_pre, S_pre, H_verse, B_verse, R, E_verse, L_verse2, X_pre],                 # 03 pre-chorus
        [K_chorus, S_chorus, H_chorus, B_chorus, P_chorus, E_chorus, L_hook_a, X_chorus],  # 04 chorus
        [K_chorus, S_chorus, H_chorus, B_chorus, P_chorus, E_chorus, L_hook_b, X_chorus],  # 05
        [K_verse, S_verse, H_verse, B_verse, R, E_verse, L_verse, X_verse],            # 06 verse 2
        [K_pre, S_pre, H_verse, B_verse, R, E_verse, L_verse2, X_pre],                 # 07 pre-chorus
        [K_chorus, S_chorus, H_chorus, B_chorus, P_chorus, E_chorus, L_hook_a, X_chorus],  # 08 chorus
        [K_chorus, S_chorus, H_chorus, B_chorus, P_chorus, E_chorus, L_hook_b, X_chorus],  # 09
        [R, R, H_intro, B_break, P_break, R, L_break, X_break],                        # 0A break
        [K_build, S_build, H_build, B_build, P_chorus, R, L_build, X_build],           # 0B build
        [K_chorus, S_chorus, H_chorus, B_chorus_up, P_chorus_up, E_chorus_up, L_hook_a_up, X_chorus_up],  # 0C in D
        [K_chorus, S_chorus, H_chorus, B_chorus_up, P_chorus_up, E_chorus_up, L_hook_b_up, X_chorus_up],  # 0D
        [K_verse, S_verse, H_verse, B_outro, P_outro, E_outro, R, X_outro],            # 0E outro
        [R, R, R, R, P_tail, R, R, X_tail],                                            # 0F last chord
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p


__all__ = ["build", "NO_INSTR"]
