"""AFTERGLOW - the song "Your First Song" builds, press by press.

Melodic house in C minor, 128 BPM, with a light swing. Built from the synth
kit every new song starts with, switched to the big engines on the way, plus
one sample from the packs and one bar resampled from the song itself:

    track 1  KICK   00 Macro Synth "kick"
    track 2  SNARE  01 Macro Synth "snare" (reverb send), a ROLL into each drop
    track 3  HATS   02 Macro Synth "hat": 16ths made with the fill tool,
                       ghost notes that play only half the time (CHNC)
    track 4  BASS   05 starter bass + a MOD slot: an LFO opening its filter
    track 5  PAD    07 HyperSynth "hyper pad", chord maj9, scale on,
                       its own EQ cuts the lows
    track 6  KEYS   09 FM4 "epiano", CHRD stabs on the off-beats
    track 7  LEAD   06 HyperSynth "trance lead": the hook
    track 8  FX     03 drums-909/crash.wav played in reverse (a riser)
                    10 rs_01.wav: the pad bar rendered to a sample, reversed

Four chords, Cm9 | Abmaj9 | Ebmaj9 | Bb9, from ONE bar of pad, bass and keys
that the chains transpose (0, -4, +3, -2).

The walkthrough (tools/walkthrough/steps.py) enters exactly this in the app;
the sim suite (first-song-walkthrough) checks that the result equals this
demo, phrase for phrase and knob for knob.
"""

from pathlib import Path

from lgpt_composer import Phrase, Project

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
KICK, SNARE, HAT, CRASH, BASS, LEAD, PAD, KEYS, REVPAD = 0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x09, 0x10

# The rest of the starter kit stays as a new song makes it
STARTER_KIT = ["kick", "snare", "hat", "openhat", "clap", "bass", "lead", "pad",
               "pluck", "keys", "bell", "acid", "subbass", "chip", "tom", "perc"]

# One bar, four chords: C, Ab (-4), Eb (+3), Bb (-2)
PROGRESSION = [0x00, 0xFC, 0x03, 0xFE]

HOOK_A = "G4 . . G4 . . A#4 . . . G4 . F4 . D#4 ."   # bars 1 and 3
HOOK_B = "D#4 . . D#4 . . G4 . . . F4 . D#4 . C4 ."  # bar 2
HOOK_C = "F4 . . F4 . . A#4 . . . C5 . A#4 . F4 ."   # bar 4, up to the top


def build() -> Project:
    p = Project("Afterglow", tempo=128)
    p.key("C", "Aeolian mode (minor)")
    p.groove(0, [7, 5])                       # swing
    # Mix: FX screen, master EQ and limiter, mixer faders
    p.params["reverb size"] = str(0xB0)       # a bigger room
    p.params["eq high gain"] = str(0x90)      # a little air on the whole mix
    p.params["limiter drive"] = str(0x40)     # louder, and never clips
    p.mixer(0, 0xFF)                          # kick up
    p.mixer(3, 0x90)                          # bass down

    for slot, preset in enumerate(STARTER_KIT):
        p.synth(slot, preset)
    p.macro(KICK, "kick")
    p.macro(SNARE, "snare", reverb=0x40)
    p.macro(HAT, "hat")
    p.sample(CRASH, ROOT / "projects/resources/samples/drums-909/crash.wav", "C3",
             volume=0x80, loopmode="reverse")
    p.synth(LEAD, "trance lead")
    # Sidechain: a "trig" envelope fired by track 1 (the kick) ducks the volume
    p.synth(PAD, "hyper pad", hyper_chord="maj9", hyper_scale=True,
            mod1_type="trig", mod1_dest="volume", mod1_amount=-64, mod1_p3=0xA0,
            eq_low_gain=0x50)
    p.synth(KEYS, "epiano")
    p.sample(REVPAD, HERE / "assets" / "afterglow-rs_01.wav", "C3", filename="rs_01.wav",
             loopmode="reverse")

    # Phrases in the order the walkthrough makes them
    kick = Phrase.drums("x...x...x...x...", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    # The fill tool copies the note, not the instrument: the track keeps I02
    hats = Phrase.drums("x...............", HAT)
    for step in range(1, 16):
        hats.set(step, note=60)
    for step in (3, 7, 11, 15):
        hats.command(step, "CHNC", 0x0080)          # ghost notes, half the time
    bass = Phrase.parse("C3 . C3 . C4 . C3 . C3 . C3 . C4 . G3 .", BASS)
    pad = Phrase.parse("C3 " + ". " * 15, PAD)
    keys = Phrase.parse(". . C3 . . . C3 . . . C3 . . . C3 .", KEYS)
    for step in (2, 6, 10, 14):
        keys.command(step, "CHRD", 0x007E)          # root, 5th, 9th
    hook_a = Phrase.parse(HOOK_A, LEAD)
    hook_b = Phrase.parse(HOOK_B, LEAD)
    hook_c = Phrase.parse(HOOK_C, LEAD)
    rest = Phrase().set(0, cmd1="KILL", param1=0)
    roll = Phrase().set(0, note=60, instr=SNARE).command(0, "ROLL", 0x0093)
    roll.command(0, "VOLM", 0x0020)
    crash = Phrase.parse("F3 " + ". " * 15, CRASH)   # 5 up: the reversed crash lasts one bar
    revpad = Phrase.parse("C3 " + ". " * 15, REVPAD)

    for ph in (kick, snare, hats, bass, pad, keys, hook_a, hook_b, hook_c, rest, roll, crash, revpad):
        p.phrase(ph)

    K = p.chain([kick] * 4)
    S = p.chain([snare] * 4)
    H = p.chain([hats] * 4)
    B = p.chain([bass] * 4, PROGRESSION)
    P = p.chain([pad] * 4, PROGRESSION)
    E = p.chain([keys] * 4, PROGRESSION)
    L = p.chain([hook_a, hook_b, hook_a, hook_c])
    R = p.chain([rest] * 4)
    U = p.chain([snare, snare, snare, roll])        # build: a snare roll into the drop
    X = p.chain([rest, rest, rest, crash])          # reversed crash into the drop
    Y = p.chain([rest, rest, rest, revpad])         # reversed pad out of the break

    rows = [
        [R, R, H, R, P, E, R, R],    # 00 intro
        [K, U, H, B, P, E, R, X],    # 01 build
        [K, S, H, B, P, E, L, R],    # 02 drop
        [K, S, H, B, P, E, L, R],    # 03
        [R, R, R, R, P, R, L, Y],    # 04 break
        [K, U, H, R, P, E, L, X],    # 05 build
        [K, S, H, B, P, E, L, R],    # 06 drop
        [R, R, H, R, P, E, R, R],    # 07 outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
