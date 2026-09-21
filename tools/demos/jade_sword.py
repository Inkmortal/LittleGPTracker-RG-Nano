"""JADE SWORD - Neon Drive reimagined as a wuxia/donghua theme, A minor, 96 BPM.

Same song map as NEON DRIVE (intro, verse, chorus, break, build, chorus,
outro) so you can compare the two projects screen by screen. What changes:
- melodies use only the five notes of the pentatonic scale (A C D E G)
- synth "guzheng": FM pluck; RTRG on long notes = tremolo picking
- synth "dizi": sine + breath noise with vibrato; "erhu": bowed saw that
  glides between notes (glide knob) for the singing slides
- taiko (a tuned-down tom) and a gong (metal wave) instead of a drum kit
"""

from lgpt_composer import Phrase, Project, note
from _patterns import arp_bar, chord_bar, crescendo, rest, voice_progression

TAIKO, CLAP, HAT, BASS, STRINGS, GUZHENG, ERHU, DIZI, GONG, WOOD = range(10)

VERSE = ["Am", "F", "C", "G"]
CHORUS = ["F", "G", "Em", "Am"]
PENTA = ["A2", "C3", "D3", "E3", "G3", "A3", "C4", "D4", "E4", "G4", "A4"]


def guzheng_flow(voicing, first_instr=True):
    """8th-note broken chord that rises and falls, like a zither hand."""
    tones = [voicing[0] - 12, voicing[0], voicing[1], voicing[2], voicing[0] + 12]
    order = [0, 2, 1, 3, 4, 3, 2, 1]
    p = Phrase()
    for k, idx in enumerate(order):
        p.set(k * 2, note=tones[idx], instr=GUZHENG)
    return p


def glissando(start=0):
    """Fast pentatonic sweep up, the classic guzheng flourish."""
    p = Phrase()
    for k, name in enumerate(PENTA[:16 - start]):
        if start + k < 16:
            p.set(start + k, note=note(name), instr=GUZHENG)
            p.command(start + k, "VOLM", 0x50 + k * 8)
    return p


def build() -> Project:
    p = Project("JadeSword", tempo=96)
    p.key("A", "Minor pentatonic")
    p.set("softclip", "Subtle")
    p.set("pregain", 60)
    p.params["reverb size"] = "215"
    p.params["reverb damp"] = "70"
    p.params["delay steps"] = "6"
    p.params["delay feedback"] = "90"

    p.synth(TAIKO, "tom", tune=-26, decay=0xB8, noise=0x28, pitch_env=0x30, reverb=0x60, volume=0xD0)
    p.synth(CLAP, "clap", decay=0xA0, reverb=0xA0, volume=0xFF)
    p.synth(HAT, "hat", decay=0x60, volume=0xB8, pan=0xA0)
    p.synth(BASS, "subbass", volume=0x70)
    p.synth(STRINGS, "pad", shape=0x40, attack=0xA8, cutoff=0x78, glide=0x04, reverb=0xA0,
            lfo_dest="pitch", lfo_rate=0x98, lfo_amount=0x06, volume=0x44)
    p.synth(GUZHENG, "keys", fm_amount=0x50, fm_ratio="2", env_amount=0xA0, env_decay=0x88,
            decay=0xC0, sustain=0, release=0xA0, pitch_env=0x04, pitch_decay=0x40,
            lfo_amount=0, reverb=0x80, delay=0x30, pan=0x60, volume=0x98)
    p.synth(ERHU, "lead", wave="saw", attack=0x58, decay=0xB0, sustain=0xC8, release=0x98,
            glide=0x50, cutoff=0x90, resonance=0x30, env_amount=0x18, drive=0x20,
            lfo_dest="pitch", lfo_rate=0xA8, lfo_amount=0x20, reverb=0x70, delay=0x30, volume=0x4C)
    p.synth(DIZI, "lead", wave="sine", shape=0x10, noise=0x28, attack=0x38, decay=0xB0,
            sustain=0xC0, release=0x90, glide=0x28, cutoff=0xC8, resonance=0x10, env_amount=0x10,
            lfo_dest="pitch", lfo_rate=0xB0, lfo_amount=0x18, reverb=0x70, delay=0x40, volume=0x60)
    p.synth(GONG, "bell", wave="metal", shape=0x20, fm_amount=0, tune=-24, attack=0x20,
            decay=0xE0, sustain=0, release=0xD0, filter="lowpass", cutoff=0x90, resonance=0x40,
            env_amount=0x30, env_decay=0xC0, reverb=0x90, volume=0x70)
    p.synth(WOOD, "perc", reverb=0x40, volume=0x70, pan=0x50)

    # --- drums -------------------------------------------------------------
    taiko_verse = Phrase.drums("x.......x...x...", TAIKO)
    taiko_chorus = Phrase.drums("x.....x.x...x.o.", TAIKO, ghost=0x70)
    taiko_fill = Phrase.drums("x.......x.x.xxxx", TAIKO)
    clap = Phrase.drums("....x.......x...", CLAP)
    hats = Phrase.drums("..x...x...x...x.", HAT)
    wood = Phrase.drums("x..x..x...x..x..", WOOD)

    rests = p.chain([rest()] * 4)
    taiko_v = p.chain([taiko_verse] * 4)
    taiko_c = p.chain([taiko_chorus, taiko_chorus, taiko_chorus, taiko_fill])
    clap_v = p.chain([clap] * 4)
    hats_c = p.chain([hats] * 4)
    wood_v = p.chain([wood] * 4)
    build_taiko = p.chain([taiko_verse, Phrase.drums("x...x...x...x...", TAIKO),
                           crescendo("x.x.x.x.x.x.x.x.", TAIKO, 0x60, 0xB0),
                           crescendo("xxxxxxxxxxxx....", TAIKO, 0x90, 0xE0)])

    gong_hit = Phrase.parse("A2 . . . . . . . . . . . . . . .", GONG)
    gong_chorus = p.chain([gong_hit, rest(), rest(), rest()])
    gong_intro = p.chain([gong_hit, Phrase(), Phrase(), Phrase()])

    # --- bass: sub notes on the roots (preset tune -24) -------------------
    def roots(names, rhythm):
        return [Phrase.parse(rhythm.replace("R", n), BASS) for n in names]
    verse_bass = p.chain(roots(["A2", "F2", "C3", "G2"], "R . . . . . . . . . R . . . . ."))
    chorus_bass = p.chain(roots(["F2", "G2", "E2", "A2"], "R . . . . . R . . . R . . . . ."))
    held_bass = p.chain(roots(["A2", "F2", "C3", "G2"], "R . . . . . . . . . . . . . . ."))

    # --- strings (whole-bar chords) and guzheng --------------------------
    verse_v = voice_progression(VERSE, "E3")
    chorus_v = voice_progression(CHORUS, "E3")
    strings_v = p.chain([chord_bar(v, STRINGS) for v in verse_v])
    strings_c = p.chain([chord_bar(v, STRINGS) for v in chorus_v])
    strings_out = p.chain([chord_bar(verse_v[0], STRINGS, ("VOLM", 0x6000)), Phrase(), Phrase(), rest()])

    zheng_v_voicing = voice_progression(VERSE, "C3")
    zheng_c_voicing = voice_progression(CHORUS, "C3")
    zheng_verse = p.chain([guzheng_flow(v) for v in zheng_v_voicing])
    zheng_chorus = p.chain([guzheng_flow(v) for v in zheng_c_voicing])
    zheng_intro = p.chain([guzheng_flow(zheng_v_voicing[0]), guzheng_flow(zheng_v_voicing[1]),
                           guzheng_flow(zheng_v_voicing[2]), glissando(8)])
    zheng_build = p.chain([guzheng_flow(v) for v in zheng_c_voicing[:3]] + [glissando(5)])

    # --- melodies (pentatonic only) -----------------------------------------
    dizi_verse = p.chain([
        Phrase.parse("E4 . . . . . . . A4 . . . G4 . E4 .", DIZI),
        Phrase.parse("D4 . . . . . . . C4 . . . . . A3 .", DIZI),
        Phrase.parse("E4 . . . . . . . G4 . . . E4 . D4 .", DIZI),
        Phrase.parse("D4 . . . . . . . . . . . - . . .", DIZI),
    ])
    erhu_a = p.chain([
        Phrase.parse("A3 . . C4 . . E4 . . . . . D4 . C4 .", ERHU),
        Phrase.parse("D4 . . . . . . . C4 . . . A3 . G3 .", ERHU),
        Phrase.parse("E4 . . . . . . . D4 . C4 . . . D4 .", ERHU),
        Phrase.parse("E4 . . D4 . C4 A3 . . . . . - . . .", ERHU),
    ])
    erhu_b = p.chain([
        Phrase.parse("A3 . . C4 . . E4 . . . G4 . E4 . D4 .", ERHU),
        Phrase.parse("D4 . . E4 . . G4 . . . . . A4 . G4 .", ERHU),
        Phrase.parse("E4 . . . D4 . E4 . G4 . . . E4 . D4 .", ERHU),
        Phrase.parse("C4 . . D4 . . A3 . . . . . - . . .", ERHU),
    ])
    # Break: guzheng alone with tremolo (RTRG) on the long notes
    trem = Phrase.parse("A3 . . C4 . . E4 . . . . . . . . .", GUZHENG)
    trem.command(6, "RTRG", 0x0002)
    trem2 = Phrase.parse("D4 . . . . . . . C4 . . . A3 . . .", GUZHENG)
    trem2.command(0, "RTRG", 0x0002)
    trem3 = Phrase.parse("E4 . . . . . . . D4 . C4 . . . . .", GUZHENG)
    trem3.command(0, "RTRG", 0x0002)
    trem4 = Phrase.parse("A3 . . . . . . . . . . . . . . .", GUZHENG)
    trem4.command(0, "RTRG", 0x0003)
    zheng_break = p.chain([trem, trem2, trem3, trem4])
    # Erhu answers from far away in the break
    erhu_break = p.chain([rest(),
                          Phrase.parse(". . . . . . . . A3:30 . . . . . . .", ERHU),
                          Phrase.parse("G3:30 . . . . . . . E3:30 . . . . . . .", ERHU),
                          Phrase.parse("A3:30 . . . . . . . . . . . - . . .", ERHU)])

    #        taiko         clap        hats/wood    bass         strings     guzheng       erhu       dizi/gong
    rows = [
        [rests, rests, rests, rests, strings_v, zheng_intro, rests, gong_intro],          # intro
        [taiko_v, rests, wood_v, held_bass, strings_v, zheng_verse, rests, rests],        # intro 2
        [taiko_v, clap_v, wood_v, verse_bass, strings_v, zheng_verse, rests, rests],      # verse
        [taiko_v, clap_v, wood_v, verse_bass, strings_v, zheng_verse, rests, dizi_verse],  # verse + dizi
        [taiko_c, clap_v, hats_c, chorus_bass, strings_c, zheng_chorus, erhu_a, gong_chorus],  # chorus
        [taiko_c, clap_v, hats_c, chorus_bass, strings_c, zheng_chorus, erhu_b, rests],   # chorus 2
        [rests, rests, rests, held_bass, strings_v, zheng_break, erhu_break, gong_intro],  # break
        [build_taiko, clap_v, wood_v, chorus_bass, strings_c, zheng_build, rests, rests],  # build
        [taiko_c, clap_v, hats_c, chorus_bass, strings_c, zheng_chorus, erhu_a, gong_chorus],  # chorus
        [taiko_c, clap_v, hats_c, chorus_bass, strings_c, zheng_chorus, erhu_b, rests],   # chorus 2
        [taiko_v, rests, wood_v, held_bass, strings_v, zheng_verse, rests, dizi_verse],   # outro
        [rests, rests, rests, rests, strings_out, zheng_intro, rests, gong_intro],         # tail
    ]
    for i, r in enumerate(rows):
        p.row(i, r)
    return p
