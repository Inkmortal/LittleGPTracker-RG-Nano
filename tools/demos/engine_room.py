"""ENGINE ROOM - every heavy sound at once, A minor, 100 BPM.

A load test you can listen to: the loudest, busiest combination the new
engines make, on all 8 tracks together, so the device's audio engine is at
its worst case (the log's [HEARTBEAT] "load peak" shows how busy it got).

    1 kick   Macro Synth (Braids KICK)
    2 snare  Macro Synth (Braids SNARE)
    3 hats   Macro Synth (Braids hat), 16ths
    4 bass   FM4 "fm bass"
    5 pad    HyperSynth "hyper pad": a 6-note chord of detuned saws
    6 keys   FM4 "epiano" + CHRD 037A: four FM voices per note
    7 lead   HyperSynth "trance lead"
    8 piano  sample chords (min7 / maj7 one-shots)

Chorus, echo and reverb sends, the master EQ and the limiter are all on.
"""

from lgpt_composer import Phrase, Project, note
from _kit import chords, use

KICK, SNARE, HAT, BASS, PAD, KEYS, LEAD, MIN7, MAJ7 = range(9)
PROG = [("A2", "min7"), ("F2", "maj7"), ("C3", "maj7"), ("G2", "min7")]
MIN7_CHRD, MAJ7_CHRD = 0x037A, 0x047B


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
    p.synth(PAD, "hyper pad", reverb=0x90, chorus=0x80, volume=0x70)
    p.synth(KEYS, "epiano", delay=0x40, reverb=0x50, volume=0x78)
    p.synth(LEAD, "trance lead", delay=0x70, reverb=0x60, chorus=0x60, volume=0x60)
    use(p, MIN7, "chords/piano-min7", 0xA0, reverb=0x40)
    use(p, MAJ7, "chords/piano-maj7", 0xA0, reverb=0x40)

    kick = Phrase.drums("x...x...x...x..x", KICK)
    snare = Phrase.drums("....x.......x...", SNARE)
    hats = Phrase.drums("xoXoxoXoxoXoxoXo", HAT, accent=0xC0, ghost=0x50)

    def bass(root: str) -> Phrase:
        low = root
        high = root[:-1] + str(int(root[-1]) + 1)
        return Phrase.parse(f"{low} . {low} . {high} . {low} . {low} . {high} . {low} . {high} .", BASS)

    def pad(root: str) -> Phrase:
        return Phrase.parse(f"{root} " + ". " * 15, PAD)

    def keys(root: str, quality: str) -> Phrase:
        # Two chord hits a bar, four FM voices each
        chrd = MIN7_CHRD if quality == "min7" else MAJ7_CHRD
        up = root[:-1] + str(int(root[-1]) + 1)
        ph = Phrase.parse(f"{up} . . . . . {up} . . . . . . . . .", KEYS)
        ph.command(0, "CHRD", chrd)
        ph.command(6, "CHRD", chrd)
        return ph

    lead_bars = [
        "E4 . . C4 . . A3 . . . C4 . D4 . E4 .",
        "F4 . . E4 . . C4 . . . A3 . C4 . . .",
        "G4 . . E4 . . C4 . . . E4 . G4 . A4 .",
        "G4 . . . D4 . . . B3 . . . D4 . . .",
    ]

    piano = [chords({"min7": MIN7, "maj7": MAJ7}, [(0, r[:-1], q), (10, r[:-1], q, 0x60)])
             for r, q in PROG]

    kicks = p.chain([kick] * 4)
    snares = p.chain([snare] * 4)
    hat_chain = p.chain([hats] * 4)
    bass_chain = p.chain([bass(r) for r, _ in PROG])
    pad_chain = p.chain([pad(r) for r, _ in PROG])
    keys_chain = p.chain([keys(r, q) for r, q in PROG])
    lead_chain = p.chain([Phrase.parse(t, LEAD) for t in lead_bars])
    piano_chain = p.chain(piano)

    # Every track plays on every row: the worst case the whole time
    full = [kicks, snares, hat_chain, bass_chain, pad_chain, keys_chain, lead_chain, piano_chain]
    for row in range(4):
        p.row(row, full)
    return p
