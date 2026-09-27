"""SUNSET CLUB - disco house in A minor, 124 BPM, made only from samples.

Every sound is a recording from the sample packs. What it shows:

- Four on the floor: the kick on every beat (steps 0 4 8 12), the clap on
  2 and 4, the open hat on every offbeat (2 6 10 14).
- Swing: groove 00 is 7/5 ticks, so every second 16th lands late, the
  shuffle that makes hats and congas bounce.
- The classic house piano: one-shot min7/maj7 chords chopped into stabs
  with a syncopated rhythm, cut short by KILL.
- The bass plays the offbeats, between the kicks, so they never clash, and
  its own EQ lifts the lows the finger-bass recording is thin on.
- Sidechain: the strings have a `trig` mod slot fired by the kick (track 1)
  that ducks them on every beat: the house "pump", on a sample.
- Instrument EQ: the piano and strings cut their lows to leave room for bass.
- A horn hook (trumpet recording) with call and response, VOLM accents,
  and a clap roll (ROLL) into each drop.
- Master EQ and limiter, mixer levels.

Sections: intro, groove, piano, horn, breakdown, build, drop x3, DJ break,
outro.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, use
from _patterns import rest

(KICK, CLAP, HAT, OPEN, BASS, MIN7, MAJ7, STR_MIN, STR_MAJ, RISER, CRASH,
 HORN, CONGA_HI, CONGA_LO, SHAKER) = range(15)
PIANO = {"min7": MIN7, "maj7": MAJ7}
PAD = {"min": STR_MIN, "maj": STR_MAJ}

PROG = [("A", "min7"), ("F", "maj7"), ("D", "min7"), ("E", "min7")]
PAD_PROG = [("A", "min"), ("F", "maj"), ("D", "min"), ("E", "min")]
STABS = (0, 3, 6, 10, 13)


def stab_bar(root: str, quality: str, busy: bool = True) -> Phrase:
    steps = STABS if busy else (0, 6)
    p = chords(PIANO, [(s, root, quality, 0xFF if s in (0, 6) else 0x98) for s in steps])
    for s in steps:
        if s + 2 < 16 and s + 2 not in steps:
            p.command(s + 2, "KILL", 0)
    return p


def bass_bar(root: str, fill: bool = False) -> Phrase:
    low, high = f"{root}1", f"{root}2"
    text = f". . {low} . . . {high} . . . {low} . . . {high} ."
    if fill:  # a walk into the next bar
        text = f". . {low} . . . {high} . . . {low} . {high} . {low} {high}"
    return Phrase.parse(text, BASS)


def build() -> Project:
    p = Project("SunsetClub", tempo=124)
    p.key("A", "Aeolian mode (minor)")
    p.groove(0, [7, 5])                     # swing
    p.set("softclip", "Subtle")
    p.set("pregain", 66)
    p.params["reverb size"] = "170"
    p.params["reverb damp"] = "100"
    p.params["delay steps"] = "3"
    p.params["delay feedback"] = "90"
    p.params["eq high gain"] = "136"        # a little sparkle on top
    p.params["eq low gain"] = "120"         # the kick is already big
    p.params["limiter drive"] = "32"
    p.mixer(KICK, 0xB0)                     # track 1
    p.mixer(3, 0xB8)                        # percussion track
    p.mixer(6, 0xB0)                        # strings behind
    p.mixer(1, 0xF0)                        # the clap forward
    p.mixer(7, 0xC0)                        # the horn on top

    # --- drums and percussion -------------------------------------------------
    use(p, KICK, "drums-909/kick", 0xA0)
    use(p, CLAP, "drums-909/clap", 0xFF, reverb=0x50)
    use(p, HAT, "drums-909/hat", 0xD8)
    use(p, OPEN, "drums-909/openhat", 0x90)
    use(p, CONGA_HI, "percussion/conga-high", 0x90, pan=0x50, reverb=0x30)
    use(p, CONGA_LO, "percussion/conga-low", 0x98, pan=0x60, reverb=0x30)
    use(p, SHAKER, "percussion/shaker", 0x70, pan=0xA8)
    use(p, RISER, "textures/riser", 0x78)
    use(p, CRASH, "drums-909/crash", 0x78)

    # --- bass: offbeats; its EQ lifts the lows the recording is thin on and
    # cuts the boxy 300 Hz where the piano and strings live ------------------
    use(p, BASS, "bass/finger", 0xE0, eq_low_gain=0xC0, eq_mid_gain=0x60, eq_mid_freq=0x30)

    # --- chords: piano stabs and string pads, lows cut --------------------------
    use(p, MIN7, "chords/piano-min7", 0xC0, reverb=0x50, eq_low_gain=0x40)
    use(p, MAJ7, "chords/piano-maj7", 0xC0, reverb=0x50, eq_low_gain=0x40)
    duck = dict(mod1_type="trig", mod1_dest="volume", mod1_amount=-80,
                mod1_p1=0x00, mod1_p2=0x20, mod1_p3=0xA0, mod1_p4=0)
    use(p, STR_MIN, "chords/strings-min", 0x98, reverb=0x70, eq_low_gain=0x50, **duck)
    use(p, STR_MAJ, "chords/strings-maj", 0x98, reverb=0x70, eq_low_gain=0x50, **duck)

    # --- the hook: a trumpet recording -------------------------------------------
    use(p, HORN, "brass/trumpet", 0xB0, reverb=0x58, delay=0x30)

    # --- patterns ----------------------------------------------------------------
    four = Phrase.drums("x...x...x...x...", KICK)
    four_out = Phrase.drums("x...x...x.......", KICK)
    clap = Phrase.drums("....x.......x...", CLAP)
    # Build: claps on the 8ths getting louder, then a ROLL that swells into
    # the drop and stops on the bar's last step (ROLL 0000)
    clap_rise = Phrase.drums("x.x.x.x.x.x.x.x.", CLAP)
    for k, i in enumerate(range(0, 16, 2)):
        clap_rise.command(i, "VOLM", 0x40 + k * 0x10)
    clap_roll = Phrase().set(0, note=60, instr=CLAP).command(0, "VOLM", 0x0060)
    clap_roll.command(0, "ROLL", 0x0093)                      # a hit every 3 ticks, louder each time
    clap_roll.set(15, cmd1="ROLL", param1=0x0000)
    hats = Phrase.drums("xoxoxoxoxoxoxoxo", HAT, ghost=0x50)
    hats8 = Phrase.drums("x.x.x.x.x.x.x.x.", HAT)
    opens = Phrase.drums("..x...x...x...x.", OPEN)
    # Congas fill the steps the open hat leaves free (it plays 2 6 10 14)
    congas = Phrase.merge(Phrase.drums("...o.x.....o.x..", CONGA_HI, ghost=0x60),
                          Phrase.drums("x......x.x......", CONGA_LO))
    groove_perc = Phrase.merge(opens, Phrase.drums(".x...x...x...x..", SHAKER, normal=0x70))
    full_perc = Phrase.merge(opens, congas)
    rests = p.chain([rest()] * 4)

    kicks = p.chain([four] * 4)
    kicks_build = p.chain([four, four, four, four_out])
    claps = p.chain([clap] * 4)
    claps_build = p.chain([clap, clap, clap_rise, clap_roll])
    hats_c = p.chain([hats] * 4)
    hats_in = p.chain([hats8] * 4)
    perc_groove = p.chain([groove_perc] * 4)
    perc_full = p.chain([full_perc] * 4)
    perc_congas = p.chain([congas] * 4)
    crash_in = Phrase.parse("C3 . . . . . . . . . . . . . . .", CRASH)
    perc_drop = p.chain([Phrase.merge(full_perc, crash_in), full_perc, full_perc, full_perc])
    riser = p.chain([rest(), rest(), rest(), Phrase.parse("C3 . . . . . . . . . . . . . . .", RISER)])

    bass = p.chain([bass_bar("A"), bass_bar("F"), bass_bar("D"), bass_bar("E", fill=True)])
    piano = p.chain([stab_bar(r, q) for r, q in PROG])
    piano_soft = p.chain([stab_bar(r, q, busy=False) for r, q in PROG])
    pads = p.chain([chords(PAD, [(0, r, q)]) for r, q in PAD_PROG])

    call = [
        Phrase.parse(". . A4 . C5 . . A4 . . E5 . D5 . C5 .", HORN),
        Phrase.parse("A4 . . . . . . . G4 . A4 . . . - .", HORN),
        Phrase.parse(". . F4 . A4 . . F4 . . D5 . C5 . A4 .", HORN),
        Phrase.parse("B4 . . . G4 . . . E4 . . . - . . .", HORN),
    ]
    # Response: the same shape, up high to close
    answer = call[:3] + [Phrase.parse("B4 . . . D5 . . . E5 . . . . . - .", HORN)]
    for ph in call + answer:
        for i, s in enumerate(ph.steps):
            if s.note != 0xFF and i in (4, 10):
                ph.command(i, "VOLM", 0xFF)      # accents on the syncopations
    horn_a = p.chain(call)
    horn_b = p.chain(answer)
    # Breakdown: the horn alone, soft, under the strings
    horn_soft = p.chain([Phrase.parse(". . A4:70 . C5:70 . . A4:70 . . E5:70 . D5:70 . C5:70 .", HORN),
                         Phrase.parse("A4:70 . . . . . . . . . . . - . . .", HORN),
                         rest(), rest()])

    rows = [
        # kick        clap         hats     perc         bass   piano       strings  horn
        [kicks, rests, hats_in, rests, rests, rests, rests, rests],              # 00 intro
        [kicks, claps, hats_c, perc_groove, rests, rests, rests, rests],         # 01 groove
        [kicks, claps, hats_c, perc_groove, bass, rests, rests, rests],          # 02 bass
        [kicks, claps, hats_c, perc_groove, bass, piano, rests, rests],          # 03 piano
        [kicks, claps, hats_c, perc_full, bass, piano, rests, horn_a],           # 04 the horn
        [kicks, claps, hats_c, perc_full, bass, piano, rests, horn_b],           # 05
        [rests, rests, rests, rests, rests, piano_soft, pads, horn_soft],        # 06 breakdown
        [kicks_build, claps_build, hats_in, riser, rests, piano, pads, rests],   # 07 build
        [kicks, claps, hats_c, perc_drop, bass, piano, pads, horn_a],            # 08 drop
        [kicks, claps, hats_c, perc_full, bass, piano, pads, horn_b],            # 09
        [kicks, claps, hats_c, perc_full, bass, piano, pads, horn_a],            # 0A
        [kicks, rests, hats_c, perc_congas, bass, rests, rests, rests],          # 0B DJ break
        [kicks, claps, hats_c, perc_groove, bass, piano_soft, pads, rests],      # 0C outro
        [kicks, rests, hats_in, rests, rests, rests, rests, rests],              # 0D
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
