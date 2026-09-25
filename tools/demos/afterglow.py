"""AFTERGLOW - the song the "Your First Song" walkthrough builds, press by press.

A minor, 122 BPM, melodic house. Made only from the synth kit every new song
starts with, with a few of its sounds switched to the big engines:

    00 KICK   -> Macro Synth "kick"        01 SNARE -> Macro Synth "snare"
    02 HAT    -> Macro Synth "hat"         05 BASS     (kept as it is)
    06 LEAD   -> HyperSynth "trance lead"  07 PAD   -> HyperSynth "hyper pad",
    09 KEYS   -> FM4 "epiano"                          chord maj9, scale on

Four chords, Am9 | Fmaj9 | Cmaj9 | G9, played by ONE bar of pad, bass and keys
that the chains transpose (0, -4, +3, -2). The pad's "scale on" turns every
note into the in-key 9th chord, so the transposed bar stays in A minor.

This file must match the walkthrough exactly: tools/walkthrough/steps.py
enters it in the app, and the sim suite (first-song-walkthrough) checks that
the result equals this demo, phrase for phrase and knob for knob.
"""

from lgpt_composer import Phrase, Project

KICK, SNARE, HAT, BASS, LEAD, PAD, KEYS = 0x00, 0x01, 0x02, 0x05, 0x06, 0x07, 0x09

# The rest of the starter kit stays as a new song makes it
STARTER_KIT = ["kick", "snare", "hat", "openhat", "clap", "bass", "lead", "pad",
               "pluck", "keys", "bell", "acid", "subbass", "chip", "tom", "perc"]

# One bar, four chords: A, F (-4), C (+3), G (-2)
PROGRESSION = [0x00, 0xFC, 0x03, 0xFE]

HOOK_A = "E4 . . E4 . . G4 . . . E4 . D4 . C4 ."   # bars 1 and 3
HOOK_B = "C4 . . C4 . . E4 . . . D4 . C4 . A3 ."   # bar 2
HOOK_C = "D4 . . D4 . . G4 . . . A4 . G4 . D4 ."   # bar 4, up to the top


def build() -> Project:
    p = Project("Afterglow", tempo=122)
    p.key("A", "Aeolian mode (minor)")

    for slot, preset in enumerate(STARTER_KIT):
        p.synth(slot, preset)
    p.macro(KICK, "kick")
    p.macro(SNARE, "snare")
    p.macro(HAT, "hat")
    p.synth(LEAD, "trance lead")
    p.synth(PAD, "hyper pad", hyper_chord="maj9", hyper_scale=True)
    p.synth(KEYS, "epiano")

    # Phrases in the order the walkthrough makes them
    kick = Phrase.drums("x...x...x...x...", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    hats = Phrase.drums("..x...x...x...x.", HAT)
    bass = Phrase.parse("A2 . A2 . A3 . A2 . A2 . A2 . A3 . E3 .", BASS)
    pad = Phrase.parse("A2 " + ". " * 15, PAD)
    keys = Phrase.parse(". . A3 . . . A3 . . . A3 . . . A3 .", KEYS)
    for step in (2, 6, 10, 14):
        keys.command(step, "CHRD", 0x007E)          # root, 5th, 9th
    hook_a = Phrase.parse(HOOK_A, LEAD)
    hook_b = Phrase.parse(HOOK_B, LEAD)
    hook_c = Phrase.parse(HOOK_C, LEAD)
    rest = Phrase().set(0, cmd1="KILL", param1=0)
    roll = Phrase().set(0, note=60, instr=SNARE).command(0, "ROLL", 0x0093)
    roll.command(0, "VOLM", 0x20)

    for ph in (kick, snare, hats, bass, pad, keys, hook_a, hook_b, hook_c, rest, roll):
        p.phrase(ph)

    kicks = p.chain([kick] * 4)
    snares = p.chain([snare] * 4)
    hat_chain = p.chain([hats] * 4)
    basses = p.chain([bass] * 4, PROGRESSION)
    pads = p.chain([pad] * 4, PROGRESSION)
    keys_chain = p.chain([keys] * 4, PROGRESSION)
    hook = p.chain([hook_a, hook_b, hook_a, hook_c])
    quiet = p.chain([rest] * 4)
    build_up = p.chain([snare, snare, snare, roll])

    K, S, H, B, P, E, L, R, U = kicks, snares, hat_chain, basses, pads, keys_chain, hook, quiet, build_up
    rows = [
        [R, R, R, R, P, E, R],    # 00 intro: the chords
        [K, U, H, B, P, E, R],    # 01 the groove comes in, snare roll into...
        [K, S, H, B, P, E, L],    # 02 the drop: everything
        [K, S, H, B, P, E, L],    # 03
        [R, R, R, R, P, R, L],    # 04 break: pad and hook
        [K, U, H, R, P, E, L],    # 05 build back up
        [K, S, H, B, P, E, L],    # 06 last drop
        [R, R, R, R, P, E, R],    # 07 outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r + [None])
    return p
