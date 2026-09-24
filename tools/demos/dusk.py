"""DUSK - the song the "Your First Song" walkthrough builds, step by step.

Only the synth kit every new song starts with, so anyone can follow along:
00 KICK, 01 SNARE, 02 HAT, 03 OPENHAT, 05 BASS, 07 PAD, 08 PLUCK.
D minor, 88 BPM, a light swing. Four bars that loop through the whole song:

    Dm7 | Bbmaj7 | Gm7 | A7

Every phrase and setting here is spelled out in docs/rgnano-wiki/Your-First-Song.md;
keep the two in step when either changes.
"""

from lgpt_composer import Phrase, Project, note

KICK, SNARE, HAT, OPENHAT, BASS, PAD, PLUCK = 0x00, 0x01, 0x02, 0x03, 0x05, 0x07, 0x08

# The chords: the note picks the root, CHRD adds the notes above it
MIN7, MAJ7, DOM7 = 0x037A, 0x047B, 0x047A
CHORDS = [("D3", MIN7), ("A#2", MAJ7), ("G2", MIN7), ("A2", DOM7)]


def kick(fill: bool) -> Phrase:
    # A bouncy kick: the beat, the "and" of 2, beat 3 (plus a pickup in bar 4)
    return Phrase.drums("x.....x...x...x." if fill else "x.....x...x.....", KICK)


def snare(fill: bool) -> Phrase:
    p = Phrase.drums("....x.......x...", SNARE)
    if fill:
        p.set(14, note=60, instr=SNARE).command(14, "VOLM", 0x50)  # ghost note
        p.set(15, note=60, instr=SNARE).command(15, "VOLM", 0x70)
    return p


def hats() -> Phrase:
    # 8ths, every off-beat softer, and an open hat on the last one
    p = Phrase()
    for step in range(0, 16, 2):
        p.set(step, note=60, instr=HAT)
        if step % 4 == 2:
            p.command(step, "VOLM", 0x60)
    p.set(14, note=60, instr=OPENHAT)
    return p


def bass(root: str, walk_up: str | None) -> Phrase:
    # Locked to the kick: root, root, octave up, then cut short
    text = f"{root} . . . . . {root} . . . {root[:-1]}{int(root[-1]) + 1} . - . . ."
    p = Phrase.parse(text, BASS)
    if walk_up:
        p.set(14, note=note(walk_up), instr=BASS)
    return p


def pad(root: str, chrd: int) -> Phrase:
    p = Phrase.parse(f"{root} " + ". " * 15, PAD)
    p.command(0, "CHRD", chrd)
    return p


HOOK = [
    "A3 . . C4 . . D4 . . . F4 . E4 . D4 .",   # Dm7
    ". . . . D4 . . . C4 . . . A3 . . .",      # Bbmaj7
    "A3 . . C4 . . D4 . . . F4 . G4 . F4 .",   # Gm7
    "E4 . . . C#4 . . . . . A3 . . . - .",     # A7: C# pulls back to D
]


def rest() -> Phrase:
    return Phrase().set(0, cmd1="KILL", param1=0)


def build() -> Project:
    p = Project("Dusk", tempo=88)
    p.key("D", "Aeolian mode (minor)")
    p.groove(0, [7, 5])                     # swing
    p.set("pregain", 70)                    # Drive 70 + Clip Subtle: headroom
    p.set("softclip", "Subtle")
    p.params["delay steps"] = "3"           # dotted 8th echo
    p.params["delay feedback"] = "112"

    p.synth(KICK, "kick")
    p.synth(SNARE, "snare", reverb=0x60)
    p.synth(HAT, "hat")
    p.synth(OPENHAT, "openhat")
    p.synth(BASS, "bass")
    p.synth(PAD, "pad", reverb=0x90, chorus=0xA0)
    p.synth(PLUCK, "pluck", delay=0x60, reverb=0x40)

    # One chain per part, four bars each (created in the order the guide does)
    kicks = p.chain([kick(False), kick(False), kick(False), kick(True)])
    snares = p.chain([snare(False), snare(False), snare(False), snare(True)])
    hat_chain = p.chain([hats()] * 4)
    basses = p.chain([bass("D3", None), bass("A#2", None), bass("G2", None), bass("A2", "C#3")])
    pads = p.chain([pad(r, c) for r, c in CHORDS])
    hook = p.chain([Phrase.parse(t, PLUCK) for t in HOOK])
    quiet = p.chain([rest()] * 4)

    #        kick   snare   hats       bass    pad   hook   -     -
    rows = [
        [quiet, quiet, quiet, quiet, pads, hook, None, None],               # 00 intro
        [kicks, quiet, hat_chain, basses, pads, quiet, None, None],         # 01 groove
        [kicks, snares, hat_chain, basses, pads, hook, None, None],         # 02 full
        [quiet, quiet, hat_chain, quiet, pads, hook, None, None],           # 03 break
        [kicks, snares, hat_chain, basses, pads, hook, None, None],         # 04 full
        [quiet, quiet, quiet, quiet, pads, quiet, None, None],              # 05 outro
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
