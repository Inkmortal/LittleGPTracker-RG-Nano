"""JADE SWORD - a wuxia / donghua theme on real Chinese instruments, A minor pentatonic, 92 BPM.

Same song map as NEON DRIVE (intro, verse, chorus, break, build, chorus,
outro) so the two projects can be compared screen by screen. The sounds are
recordings (projects/resources/samples/chinese, credits in CREDITS.md):
- guzheng: flowing broken chords and pentatonic glissandos
- pipa: tremolo picking (RTRG) on the chorus counter-melody
- erhu and dizi: the singing lines, with grace notes and trills
- dagu drum, Beijing-opera clapper drum, small gong, cymbals and the big
  opera gong instead of a drum kit
A soft synth pad and sub bass sit underneath. Melodies use only the five
notes of the pentatonic scale (A C D E G).
"""

from pathlib import Path

from lgpt_composer import Phrase, Project, note
from _patterns import chord_bar, rest, voice_progression

SAMPLES = Path(__file__).resolve().parents[2] / "projects" / "resources" / "samples" / "chinese"

(DRUM, RIM, BANGU, XIAOLUO, NAOBO, WOOD, BASS, STRINGS,
 GUZHENG, PIPA, ERHU, ERHU_LOW, DIZI, DIZI_HIGH, GONG) = range(15)

VERSE = ["Am", "F", "C", "G"]
CHORUS = ["F", "G", "Em", "Am"]
PENTA = ["A2", "C3", "D3", "E3", "G3", "A3", "C4", "D4", "E4", "G4", "A4", "C5", "D5", "E5", "G5", "A5"]


def pick(n: int, low: int, high: int, split: str) -> int:
    """Two recordings of one instrument: use the one recorded closer to the note."""
    return high if n >= note(split) else low


def line(text: str, low: int, high: int, split: str) -> Phrase:
    """Melody where each note plays on the nearer of two recordings."""
    p = Phrase.parse(text, None)
    for i, s in enumerate(p.steps):
        if s.note != 0xFF:
            p.set(i, instr=pick(s.note, low, high, split))
    return p


def erhu(text: str) -> Phrase:
    return line(text, ERHU_LOW, ERHU, "G3")


def dizi(text: str) -> Phrase:
    return line(text, DIZI, DIZI_HIGH, "G4")


def zheng_flow(voicing) -> Phrase:
    """8th-note broken chord that rises and falls, like a zither hand."""
    tones = [voicing[0] - 12, voicing[0], voicing[1], voicing[2], voicing[0] + 12]
    order = [0, 2, 1, 3, 4, 3, 2, 1]
    p = Phrase()
    for k, idx in enumerate(order):
        p.set(k * 2, note=tones[idx], instr=GUZHENG)
        if k % 2:
            p.command(k * 2, "VOLM", 0x60)
    return p


def glissando(start: int = 0, first: int = 3) -> Phrase:
    """Fast pentatonic sweep up the strings, the classic guzheng flourish."""
    p = Phrase()
    for k, name in enumerate(PENTA[first:first + 16 - start]):
        p.set(start + k, note=note(name), instr=GUZHENG)
        p.command(start + k, "VOLM", min(0xFF, 0x48 + k * 12))
    return p


def pipa_tremolo(text: str) -> Phrase:
    """Pipa lunzhi: every note is a fast tremolo."""
    p = Phrase.parse(text, PIPA)
    for i, s in enumerate(p.steps):
        if s.note != 0xFF:
            p.command(i, "RTRG", 0x0002)
    return p


def luogu(pattern: dict[int, str]) -> Phrase:
    """Beijing-opera percussion on one track: b bangu, s small gong, c cymbals."""
    kinds = {"b": BANGU, "s": XIAOLUO, "c": NAOBO}
    p = Phrase()
    for i, k in pattern.items():
        p.set(i, note=60, instr=kinds[k])
    return p


def build() -> Project:
    p = Project("JadeSword", tempo=92)
    p.key("A", "Minor pentatonic")
    p.set("softclip", "Subtle")
    p.set("pregain", 70)
    p.params["reverb size"] = "215"
    p.params["reverb damp"] = "80"
    p.params["delay steps"] = "6"
    p.params["delay feedback"] = "80"

    # --- recordings ---------------------------------------------------------
    p.sample(DRUM, SAMPLES / "drum.wav", "C3", volume=0xB0, reverb=0x40)
    p.sample(RIM, SAMPLES / "drum-rim.wav", "C3", volume=0x70, reverb=0x30, pan=0x90)
    p.sample(BANGU, SAMPLES / "bangu.wav", "C3", volume=0xB0, pan=0x60, reverb=0x30)
    p.sample(XIAOLUO, SAMPLES / "xiaoluo.wav", "C3", volume=0x90, pan=0x98, reverb=0x50)
    p.sample(NAOBO, SAMPLES / "naobo.wav", "C3", volume=0x80, reverb=0x50)
    p.sample(WOOD, SAMPLES / "woodblock.wav", "C3", volume=0x90, pan=0x50, reverb=0x30)
    p.sample(GUZHENG, SAMPLES / "guzheng.wav", "A2", volume=0xB0, pan=0x60, reverb=0x70, delay=0x28)
    p.sample(PIPA, SAMPLES / "pipa.wav", "A3", volume=0xB0, pan=0x98, reverb=0x60)
    p.sample(ERHU, SAMPLES / "erhu.wav", "D4", volume=0x88, reverb=0x70, delay=0x30)
    p.sample(ERHU_LOW, SAMPLES / "erhu-low.wav", "D3", volume=0x88, reverb=0x70, delay=0x30)
    p.sample(DIZI, SAMPLES / "dizi.wav", "E4", volume=0x98, reverb=0x80, delay=0x40)
    p.sample(DIZI_HIGH, SAMPLES / "dizi-a.wav", "A4", volume=0x98, reverb=0x80, delay=0x40)
    p.sample(GONG, SAMPLES / "gong.wav", "C3", volume=0x78, reverb=0x80)

    # CC BY recordings: the credits travel with the song
    p.extra_files["CREDITS.md"] = SAMPLES / "CREDITS.md"

    # --- synth bed ------------------------------------------------------------
    p.synth(BASS, "subbass", volume=0x50)
    p.synth(STRINGS, "pad", shape=0x40, attack=0xB0, cutoff=0x68, glide=0x04, reverb=0xA0,
            lfo_dest="pitch", lfo_rate=0x98, lfo_amount=0x06, volume=0x34)

    # --- drums ----------------------------------------------------------------
    drum_verse = Phrase.merge(Phrase.drums("x.......x...x...", DRUM),
                              Phrase.drums("....o.......o...", RIM, ghost=0x60))
    drum_chorus = Phrase.merge(Phrase.drums("x.....x.x...x...", DRUM),
                               Phrase.drums("....x.......x..o", RIM, ghost=0x50))
    drum_fill = Phrase.drums("x.......x.x.xxxx", DRUM)
    roll = Phrase.drums("xxxxxxxxxxxxxxxx", DRUM)
    for i in range(16):
        roll.command(i, "VOLM", 0x30 + i * 12)

    wood = Phrase.drums("x..x..x...x..x..", WOOD)
    luogu_verse = luogu({0: "b", 2: "b", 4: "c", 8: "s", 10: "b", 12: "c"})
    luogu_chorus = luogu({0: "b", 2: "b", 3: "b", 4: "c", 6: "b", 8: "s", 10: "b", 11: "b", 12: "c", 14: "s"})
    luogu_run = luogu({0: "b", 1: "b", 2: "b", 3: "b", 4: "b", 5: "b", 6: "b", 7: "b",
                       8: "s", 10: "s", 12: "c", 14: "c"})

    rests = p.chain([rest()] * 4)
    drum_v = p.chain([drum_verse] * 4)
    drum_c = p.chain([drum_chorus, drum_chorus, drum_chorus, drum_fill])
    drum_build = p.chain([drum_verse, drum_verse, drum_fill, roll])
    wood_v = p.chain([wood] * 4)
    opera_v = p.chain([luogu_verse] * 4)
    opera_c = p.chain([luogu_chorus, luogu_chorus, luogu_chorus, luogu_run])
    opera_build = p.chain([luogu_verse, luogu_verse, luogu_run, luogu_run])

    gong_hit = Phrase.parse("C3 . . . . . . . . . . . . . . .", GONG)
    gong_chorus = p.chain([gong_hit, rest(), rest(), rest()])
    gong_intro = p.chain([gong_hit, Phrase(), Phrase(), Phrase()])

    # --- bass: sub notes on the roots ------------------------------------------
    def roots(names, rhythm):
        return [Phrase.parse(rhythm.replace("R", n), BASS) for n in names]
    verse_bass = p.chain(roots(["A2", "F2", "C3", "G2"], "R . . . . . . . . . R . . . . ."))
    chorus_bass = p.chain(roots(["F2", "G2", "E2", "A2"], "R . . . . . R . . . R . . . . ."))
    held_bass = p.chain(roots(["A2", "F2", "C3", "G2"], "R . . . . . . . . . . . . . . ."))

    # --- pad and guzheng ----------------------------------------------------------
    verse_v = voice_progression(VERSE, "E3")
    chorus_v = voice_progression(CHORUS, "E3")
    strings_v = p.chain([chord_bar(v, STRINGS) for v in verse_v])
    strings_c = p.chain([chord_bar(v, STRINGS) for v in chorus_v])
    strings_out = p.chain([chord_bar(verse_v[0], STRINGS, ("VOLM", 0x6000)), Phrase(), Phrase(), rest()])

    zheng_v = voice_progression(VERSE, "C3")
    zheng_c = voice_progression(CHORUS, "C3")
    zheng_verse = p.chain([zheng_flow(v) for v in zheng_v])
    zheng_chorus = p.chain([zheng_flow(v) for v in zheng_c])
    zheng_intro = p.chain([zheng_flow(zheng_v[0]), zheng_flow(zheng_v[1]),
                           zheng_flow(zheng_v[2]), glissando(6)])
    zheng_build = p.chain([zheng_flow(v) for v in zheng_c[:3]] + [glissando(2, 1)])

    # --- melodies: grace notes (a quick step into the note) and trills --------------
    dizi_verse = p.chain([
        dizi("D4 E4 . . . . . . G4 A4 . . G4 . E4 ."),
        dizi("C4 D4 . . . . . . C4 . . . . . A3 ."),
        dizi("D4 E4 . . . . . . G4 . A4 G4 E4 . D4 ."),
        dizi("E4 D4 E4 D4 E4 . . . . . . . - . . ."),
    ])
    erhu_a = p.chain([
        erhu("A3 . . C4 . . D4 E4 . . . . D4 . C4 ."),
        erhu("C4 D4 . . . . . . C4 . . . A3 . G3 ."),
        erhu("D4 E4 . . . . . . D4 . C4 . . . D4 ."),
        erhu("E4 . . D4 . C4 A3 . . . . . - . . ."),
    ])
    erhu_b = p.chain([
        erhu("A3 . . C4 . . D4 E4 . . G4 . E4 . D4 ."),
        erhu("D4 . . E4 . . G4 . . . . . A4 . G4 ."),
        erhu("D4 E4 . . D4 . E4 . G4 . . . E4 . D4 ."),
        erhu("C4 . . D4 . . G3 A3 . . . . - . . ."),
    ])
    # Pipa tremolo counter-melody under the second chorus
    pipa_c = p.chain([
        pipa_tremolo("A3 . . . . . . . C4 . . . . . . ."),
        pipa_tremolo("D4 . . . . . . . E4 . . . . . . ."),
        pipa_tremolo("G3 . . . . . . . E3 . . . . . . ."),
        pipa_tremolo("A3 . . . . . . . . . . . - . . ."),
    ])
    # Break: guzheng alone with tremolo on the long notes, erhu from afar
    trem = Phrase.parse("A3 . . C4 . . E4 . . . . . . . . .", GUZHENG)
    trem.command(6, "RTRG", 0x0002)
    trem2 = Phrase.parse("D4 . . . . . . . C4 . . . A3 . . .", GUZHENG)
    trem2.command(0, "RTRG", 0x0002)
    trem3 = Phrase.parse("E4 . . . . . . . D4 . C4 . . . . .", GUZHENG)
    trem3.command(0, "RTRG", 0x0002)
    trem4 = Phrase.parse("A3 . . . . . . . . . . . . . . .", GUZHENG)
    trem4.command(0, "RTRG", 0x0003)
    zheng_break = p.chain([trem, trem2, trem3, trem4])
    erhu_break = p.chain([rest(),
                          erhu(". . . . . . . . G3:50 A3:50 . . . . . ."),
                          erhu("G3:50 . . . . . . . E3:50 . . . . . . ."),
                          erhu("G3:50 A3:50 . . . . . . . . . . - . . .")])

    #        drum         opera        wood/pipa    bass         pad         guzheng       erhu        dizi/gong
    rows = [
        [rests, rests, rests, rests, strings_v, zheng_intro, rests, gong_intro],               # intro
        [drum_v, rests, wood_v, held_bass, strings_v, zheng_verse, rests, rests],               # intro 2
        [drum_v, opera_v, wood_v, verse_bass, strings_v, zheng_verse, rests, rests],            # verse
        [drum_v, opera_v, wood_v, verse_bass, strings_v, zheng_verse, rests, dizi_verse],       # verse + dizi
        [drum_c, opera_c, rests, chorus_bass, strings_c, zheng_chorus, erhu_a, gong_chorus],    # chorus
        [drum_c, opera_c, pipa_c, chorus_bass, strings_c, zheng_chorus, erhu_b, rests],         # chorus 2
        [rests, rests, rests, held_bass, strings_v, zheng_break, erhu_break, gong_intro],       # break
        [drum_build, opera_build, wood_v, chorus_bass, strings_c, zheng_build, rests, rests],   # build
        [drum_c, opera_c, rests, chorus_bass, strings_c, zheng_chorus, erhu_a, gong_chorus],    # chorus
        [drum_c, opera_c, pipa_c, chorus_bass, strings_c, zheng_chorus, erhu_b, rests],         # chorus 2
        [drum_v, rests, wood_v, held_bass, strings_v, zheng_verse, rests, dizi_verse],          # outro
        [rests, rests, rests, rests, strings_out, zheng_intro, rests, gong_intro],               # tail
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
