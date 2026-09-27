"""ENGINE ROOM - every heavy sound at once, A minor, 100 BPM.

A load test you can listen to: the loudest, busiest combination the new
engines make, on all 8 tracks together, so the device's audio engine is at
its worst case (the log's [HEARTBEAT] "load peak" shows how busy it got).
Every track plays on every row: the worst case the whole time.

    1 kick   Macro Synth (Braids KICK)
    2 snare  Macro Synth (Braids SNARE)
    3 hats   Macro Synth (Braids hat), 16ths
    4 bass   FM4 "fm bass": offbeats, between the kicks
    5 pad    HyperSynth "hyper pad": a 6-note chord of detuned saws, no
             sub, its own EQ cuts the lows
    6 keys   FM4 "epiano" + CHRD 037A: four FM voices per note
    7 lead   HyperSynth "trance lead", above the keys
    8 piano  sample chords (min7 / maj7 one-shots), lows cut

Chorus, echo and reverb sends, the master EQ and the limiter are all on.
Two sections: A (Am F C G) and B (F G Em Am, the lead an octave up), then
both again with a turnaround. tools/dsp-harness/engine_room_check.cpp
measures the device cost of the first two rows of this song.
"""

from lgpt_composer import Phrase, Project
from _kit import chords, use

KICK, SNARE, HAT, BASS, PAD, KEYS, LEAD, MIN7, MAJ7 = range(9)
PROG_A = [("A2", "min7"), ("F2", "maj7"), ("C3", "maj7"), ("G2", "min7")]
PROG_B = [("F2", "maj7"), ("G2", "min7"), ("E2", "min7"), ("A2", "min7")]
MIN7_CHRD, MAJ7_CHRD = 0x037A, 0x047B


def up(name: str, octaves: int = 1) -> str:
    return name[:-1] + str(int(name[-1]) + octaves)


def build() -> Project:
    p = Project("EngineRoom", tempo=100)
    p.key("A", "Aeolian mode (minor)")
    p.set("softclip", "Subtle")
    p.params["chorus depth"] = "150"
    p.params["eq low gain"] = "150"     # +2 dB of weight
    p.params["eq high gain"] = "140"
    p.params["limiter drive"] = "80"    # the limiter works all the time
    p.params["limiter ceiling"] = "252"

    p.macro(KICK, "kick")
    p.macro(SNARE, "snare", reverb=0x50)
    p.macro(HAT, "hat")
    p.synth(BASS, "fm bass", volume=0xB0)
    p.synth(PAD, "hyper pad", hyper_sub=0, reverb=0x90, chorus=0x80, volume=0x70, eq_low_gain=0x50)
    p.synth(KEYS, "epiano", delay=0x40, reverb=0x50, volume=0x78)
    p.synth(LEAD, "trance lead", delay=0x70, reverb=0x60, chorus=0x60, volume=0x60)
    use(p, MIN7, "chords/piano-min7", 0xA0, reverb=0x40, eq_low_gain=0x40)
    use(p, MAJ7, "chords/piano-maj7", 0xA0, reverb=0x40, eq_low_gain=0x40)

    kick = Phrase.drums("x...x...x...x..x", KICK)
    kick_turn = Phrase.drums("x...x...x.x.x.xx", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    snare_turn = Phrase.drums("....x.......x.xX", SNARE, accent=0xFF, ghost=0x70)
    hats = Phrase.drums("xoXoxoXoxoXoxoXo", HAT, accent=0xC0, ghost=0x50)

    def bass(root: str) -> Phrase:
        # Between the kicks (steps 2 6 10 14) with an octave pop, like the
        # Afterglow bass, so the FM bass and the kick don't blur
        return Phrase.parse(f". . {root} . . . {up(root)} {root} . . {root} . . . {up(root)} .", BASS)

    def pad(root: str) -> Phrase:
        return Phrase.parse(f"{root} " + ". " * 15, PAD)

    def keys(root: str, quality: str) -> Phrase:
        # Two chord hits a bar, four FM voices each
        chrd = MIN7_CHRD if quality == "min7" else MAJ7_CHRD
        ph = Phrase.parse(f"{up(root)} . . . . . {up(root)} . . . . . . . . .", KEYS)
        ph.command(0, "CHRD", chrd)
        ph.command(6, "CHRD", chrd)
        return ph

    lead_a = [
        "E5 . . C5 . . A4 . . . C5 . D5 . E5 .",
        "F5 . . E5 . . C5 . . . A4 . C5 . . .",
        "G5 . . E5 . . C5 . . . E5 . G5 . A5 .",
        "G5 . . . D5 . . . B4 . . . D5 . . .",
    ]
    lead_b = [
        "A5 . . G5 . . F5 . . . E5 . F5 . A5 .",
        "B5 . . . G5 . . . D5 . . . G5 . . .",
        "G5 . . E5 . . B4 . . . E5 . G5 . B5 .",
        "A5 . . . . . E5 . C5 . . . A4 . . .",
    ]

    def piano(prog):
        return [chords({"min7": MIN7, "maj7": MAJ7}, [(0, r[:-1], q), (10, r[:-1], q, 0x60)])
                for r, q in prog]

    kicks = p.chain([kick] * 4)
    kicks_turn = p.chain([kick, kick, kick, kick_turn])
    snares = p.chain([snare] * 4)
    snares_turn = p.chain([snare, snare, snare, snare_turn])
    hat_chain = p.chain([hats] * 4)
    bass_a = p.chain([bass(r) for r, _ in PROG_A])
    bass_b = p.chain([bass(r) for r, _ in PROG_B])
    pad_a = p.chain([pad(r) for r, _ in PROG_A])
    pad_b = p.chain([pad(r) for r, _ in PROG_B])
    keys_a = p.chain([keys(r, q) for r, q in PROG_A])
    keys_b = p.chain([keys(r, q) for r, q in PROG_B])
    lead_a_chain = p.chain([Phrase.parse(t, LEAD) for t in lead_a])
    lead_b_chain = p.chain([Phrase.parse(t, LEAD) for t in lead_b])
    piano_a = p.chain(piano(PROG_A))
    piano_b = p.chain(piano(PROG_B))

    a = [kicks, snares, hat_chain, bass_a, pad_a, keys_a, lead_a_chain, piano_a]
    a_turn = [kicks_turn, snares_turn, hat_chain, bass_a, pad_a, keys_a, lead_a_chain, piano_a]
    b = [kicks, snares, hat_chain, bass_b, pad_b, keys_b, lead_b_chain, piano_b]
    b_turn = [kicks_turn, snares_turn, hat_chain, bass_b, pad_b, keys_b, lead_b_chain, piano_b]
    for row, chains in enumerate([a, a_turn, b, b_turn, a, b_turn]):
        p.row(row, chains)
    return p
