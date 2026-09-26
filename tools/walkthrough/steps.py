"""Your First Song, one button press at a time.

Each Step is exactly what you press (keys) and what you should see (say).
tools/make_walkthrough.py turns this list into:
- the simulator test first-song-walkthrough.rgsim (ends by checking that the
  song equals the Afterglow demo, tools/demos/afterglow.py),
- a screenshot after every step, drawn above the RG Nano's buttons,
- docs/rgnano-wiki/Your-First-Song.md.

keys: buttons joined with '+' (all but the last are held down), then an
optional ' xN' for N taps of the last one:
    "A"   "Down x4"   "RB+Right"   "A+Right x15"   "B+A"   ""  (no input)
Buttons: Up Down Left Right A B LB RB Start Select.

say: one or two short sentences, written so they still work without the
picture (the app's built-in guide is text only). The generator puts the keys
in front of it.

expect: screen text the test checks after the step (catches a drifting app
early, next to the step that drifted).
"""

from __future__ import annotations

from dataclasses import dataclass, field

DEMO_NAME = "Afterglow"


@dataclass
class Step:
    keys: str
    say: str
    wait: int = 0                       # ms before the screenshot (0 = settle only)
    expect: list[str] = field(default_factory=list)


@dataclass
class Section:
    title: str
    text: str
    steps: list[Step]


def S(keys: str, say: str, wait: int = 0, expect: str | list[str] | None = None) -> Step:
    if expect is None:
        expect = []
    elif isinstance(expect, str):
        expect = [expect]
    return Step(keys, say, wait, list(expect))


LISTEN = 2600  # ms of playback before the screenshot of a "listen" step


INTRO = """
# Your First Song

You'll build **Afterglow**, a melodic house track in C minor at 128 BPM: punchy Macro Synth drums with swung 16th hats, a rolling bass, huge HyperSynth chords that pump with the kick, FM electric piano stabs, a trance lead with a hook that sticks, snare rolls and reversed cymbals into every drop, and a proper arrangement: intro, build, drop, break, build, drop, outro.

On the way you use nearly everything the app can do: the synth engines and the Macro Synth, a sample from the packs, commands, the fill and random tools, modulation, EQ and effects, resampling, undo, bookmarks, the mixer, Live mode and export. Plan on an hour or two; every chapter ends with something new to listen to.

Want to hear where you're going first? **Afterglow** is in the [Demo Songs](Demo-Songs): open it and press **Start**.

**How to follow along.** Every step is one thing to press, with a picture of the screen right after it. Under each screen the RG Nano's buttons are drawn: the button to press is **orange**, a button to hold down first is **cream** and says `HOLD`, and `x4` means press it 4 times. So **RB + Right** means: hold **RB**, press **Right**, let go of both.

Lost? **RB + Select** opens the helper on any screen, and **B + Select** undoes your last change.
"""

OUTRO = """
## Where next

- **Make it yours:** change a note of the hook (**A + Left/Right** on it), try other transposes in the chains for a new chord progression, or another `preset` on any instrument.
- **More randomness:** put `RAND 0003` next to a few hook notes: they wander a little every time.
- Pull the other [Demo Songs](Demo-Songs) apart, and keep the [Music Theory Cheat Sheet](Music-Theory-Cheat-Sheet) nearby. Every key is on the [Controls](Controls) page.
"""


def rows_of(chain: str, phrase: str) -> list[Step]:
    """Rows 1-3 of a new chain: the same phrase again (A reuses it)."""
    out = []
    for row in (1, 2, 3):
        out.append(S("Down", f"Row `{row}`."))
        out.append(S("A", f"Phrase `{phrase}`" + (f": four bars of chain `{chain}`." if row == 3 else ".")))
    return out


def transposes(done: str) -> list[Step]:
    """Row 3, 2, 1 transposes of a chord chain: FE, 03, FC."""
    return [
        S("Right", "Row `3`'s transpose, `00`."),
        S("A+Left x2", "`FE`: two semitones down."),
        S("Up", "Row `2`'s transpose."),
        S("A+Right x3", "`03`: three up."),
        S("Up", "Row `1`'s transpose."),
        S("A+Left x4", f"`FC`: four down. {done}"),
    ]


SECTIONS: list[Section] = [
    # ------------------------------------------------------------------ start
    Section("1. A new song", """
The start screen lists your songs. You'll make a new one and call it GLOW.
""", [
        S("", "The app opens on **Your Songs**. Along the bottom are four buttons; `Open` is lit."),
        S("Right", "`New` is lit. The line under the buttons says what **A** will do."),
        S("A", "The **New song** box, with a made-up name typed in and `DONE` lit. Typing a letter replaces the name.",
          expect="NEW SONG"),
        S("Down", "The cursor jumps up to the letters, on `J`."),
        S("Left x3", "`G`."),
        S("A", "`G` replaces the suggested name."),
        S("Down", "`Q`, one row down."),
        S("Left x5", "`L`."),
        S("A", "`GL`."),
        S("Right x3", "`O`."),
        S("A", "`GLO`."),
        S("Down", "`Y`."),
        S("Left x2", "`W`."),
        S("A", "`GLOW`.", expect="GLOW"),
        S("Start", "**Start** creates the song. This is the **Song** screen: 8 tracks side by side, time running down.",
          wait=800, expect="Song - GLOW"),
        S("RB+Select", "The **helper**. Its map shows where you are (`SONG`) and which screen **RB** + each direction takes you to. It works on every screen.",
          expect="MAP"),
        S("RB+Select", "The helper closes."),
    ]),
    Section("2. Tempo and key", """
The Project screen holds the song's settings. You'll set the tempo and the key. With a key set, editing a note only walks through the notes of that key, so a wrong note is hard to hit.
""", [
        S("RB+Up", "The **Project** screen, which sits above Song on the map.", expect="Tempo"),
        S("Down x3", "The cursor is on `Tempo: 138 bpm`."),
        S("A+Down", "Holding **A**, **Down** takes 10 off: `128 bpm`, a house tempo. (**A + Left/Right** changes it by 1.)",
          expect="128 bpm"),
        S("Down x2", "The cursor is on `Key: --`."),
        S("A+Right", "`Key: C`."),
        S("Down", "The cursor is on `Scale: None (Chromatic)`."),
        S("A+Right x3", "`Scale: Aeolian mode (minor)`. The song is in **C minor**.", expect="Aeolian"),
        S("RB+Right", "The **Scale** screen draws the key on a keyboard: the lit keys are the notes of C minor.",
          expect="Aeolian"),
        S("RB+Left", "Back on Project."),
        S("RB+Down", "Back on the Song screen."),
    ]),
    # ------------------------------------------------------------------ drums
    Section("3. The kick", """
A song is built from three kinds of blocks:

- a **phrase** is one bar: 16 steps, each with a note, an instrument and up to two commands;
- a **chain** is a list of phrases played one after the other. Our chains are 4 bars;
- the **Song** screen plays one chain per track, row after row.

You'll start with a kick on every beat, in track 1.
""", [
        S("A", "Chain `00` goes into track 1, row `00`."),
        S("RB+Right", "The **Chain** screen of chain `00`. Each row holds one phrase; it's empty."),
        S("A", "Phrase `00` goes into row `0`."),
        S("RB+Right", "The **Phrase** screen of phrase `00`: 16 steps, `0` to `F`, top to bottom. Each step is a 16th note."),
        S("A", "A note: `C 3` played by instrument `I00`, the **KICK**. You hear it as it goes in.", expect="KICK"),
        S("Down x4", "Step `4`, the second beat."),
        S("A", "Another kick: **A** on an empty step copies the last note."),
        S("Down x4", "Step `8`, beat 3."),
        S("A", "Kick."),
        S("Down x4", "Step `C`, beat 4."),
        S("A", "Kick. Four on the floor."),
        S("Start", "The bar plays on a loop: `PLAY:PHR`, and a marker runs down the steps.", wait=LISTEN),
        S("Start", "Stop."),
    ]),
    Section("4. A bigger kick: the Macro Synth", """
`I00` is a plain synth kick, from the kit every new song starts with. The **Macro Synth** has a much punchier one. Switching an instrument over is the same few moves for every sound in this song.
""", [
        S("RB+Right", "The **Instrument** screen of `I00`, the KICK. The cursor is on `preset`.", expect="KICK"),
        S("Up x2", "The cursor is on `type   synth`."),
        S("A+Right", "`type   macro`: a Macro Synth, starting from its plain `init` sound.", expect="MACRO"),
        S("Down", "The cursor is on `preset init`."),
        S("A+Right x15", "`preset kick`. (Holding **A** and **Right** keeps going until you let go.)", expect="kick"),
        S("Start", "The phrase again, with the new kick. **Start** plays the phrase from the Instrument screen too.",
          wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on phrase `00`."),
        S("RB+Left", "Back on chain `00`."),
        *rows_of("00", "00"),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("5. Snare, with reverb", """
Track 2: a snare on beats 2 and 4. It needs its own chain and phrase: on an empty spot **A** reuses the last one, and **A** again makes a new one.

The Chain and Phrase screens keep the cursor where you left it, even in a new chain or phrase, so some steps just move it back.
""", [
        S("Right", "Track 2, row `00`."),
        S("A", "Chain `00` goes in: **A** reuses the last chain.", expect="Reused 00"),
        S("A", "**A** again: a brand-new chain `01`.", expect="New chain 01"),
        S("RB+Right", "Chain `01`, empty. The cursor is on row `3`, where you left chain `00`."),
        S("Up x3", "Row `0`."),
        S("A", "Phrase `00` goes in."),
        S("A", "A new phrase `01`.", expect="New phrase 01"),
        S("RB+Right", "Phrase `01`, empty. The cursor is on step `C`, where you left the kick: beat 4, just right."),
        S("A", "`C 3 I00`, a copy of the kick. The next steps turn it into a snare."),
        S("Right", "The cursor is on the instrument, `I00`."),
        S("A+Right", "`I01`, the **SNARE**. Every note picks its own instrument.", expect="SNARE"),
        S("Left", "Back on the note."),
        S("Up x8", "Step `4`, beat 2."),
        S("A", "Snare (a copy of `C 3 I01`)."),
        S("RB+Right", "Instrument `I01`, the SNARE.", expect="SNARE"),
        S("Up x2", "The cursor is on `type`."),
        S("A+Right", "`type   macro`.", expect="MACRO"),
        S("Down", "The cursor is on `preset init`."),
        S("A+Right x16", "`preset snare`.", expect="snare"),
        S("LB+Right x5", "**LB + Left/Right** flips through the instrument's pages. Page 6, **MIX**: volume, pan and the three effect sends.",
          expect="MIX"),
        S("Down x2", "The cursor is on `reverb 00`."),
        S("A+Up x4", "`reverb 40`: the snare now sends a little of itself into the reverb. **A + Up/Down** moves in big steps.",
          expect="40"),
        S("LB+Left x5", "Back on page 1, so the next instrument you open starts there too."),
        S("RB+Left", "Back on phrase `01`."),
        S("RB+Left", "Back on chain `01`."),
        *rows_of("01", "01"),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("6. Hi-hats: fill and chance", """
Track 3: a hi-hat on every 16th. You'll enter one and let the **fill** tool copy it down the bar. Then every 4th hat gets `CHNC 0080`: it plays only half the time, so the pattern never loops exactly the same.
""", [
        S("Right", "Track 3, row `00`."),
        S("A", "Chain `01` goes in."),
        S("A", "A new chain `02`.", expect="New chain 02"),
        S("RB+Right", "Chain `02`, cursor on row `3`."),
        S("Up x3", "Row `0`."),
        S("A", "Phrase `01` goes in."),
        S("A", "A new phrase `02`.", expect="New phrase 02"),
        S("RB+Right", "Phrase `02`, empty, cursor on step `4`."),
        S("Up x4", "Step `0`."),
        S("A", "`C 3 I01`, a copy of the snare."),
        S("Right", "The cursor is on `I01`."),
        S("A+Right", "`I02`, the **HAT**.", expect="HAT"),
        S("Left", "Back on the note."),
        S("B+LB", "**B + LB** starts a selection on step `0`."),
        S("Down x15", "The selection covers the whole bar."),
        S("LB+Left", "**LB + Left** fills: the first selected note on every step. (Again: every 2nd step, then every 4th.)"),
        S("B", "**B** copies the selection and ends it; the cursor goes back to step `0`."),
        S("Down x3", "Step `3`."),
        S("Right x2", "The first command column."),
        S("Select", "**Select** opens the command list. The text under it explains the command under the cursor.",
          expect="ARPG"),
        S("Right", "`CHNC`: the chance that the note plays."),
        S("A", "`CHNC 0000` goes into step `3`.", expect="CHNC"),
        S("Right", "The cursor is on the value; its last digit is lit."),
        S("A+Left", "Holding **A**, **Left** lights the next digit."),
        S("A+Up x8", "`0080`: half the time (`FF` = always)."),
        S("Left", "Back on `CHNC`."),
        S("B+LB", "A selection on the command."),
        S("Right", "It grows over the value too."),
        S("B", "Copied.", expect="Copied"),
        S("Down x4", "Step `7`."),
        S("A+LB", "**A + LB** pastes `CHNC 0080` here, and the cursor moves on to step `8`."),
        S("Down x3", "Step `B`."),
        S("A+LB", "Pasted."),
        S("Down x3", "Step `F`."),
        S("A+LB", "Pasted."),
        S("RB+Right", "Instrument `I02`, the HAT.", expect="HAT"),
        S("Up x2", "The cursor is on `type`."),
        S("A+Right", "`type   macro`.", expect="MACRO"),
        S("Down", "The cursor is on `preset init`."),
        S("A+Right x17", "`preset hat`.", expect="hat"),
        S("Start", "Listen: some of the quiet in-between hats come and go.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on phrase `02`."),
        S("RB+Left", "Back on chain `02`."),
        *rows_of("02", "02"),
        S("RB+Left", "Back on the Song screen."),
        S("Start", "The song plays from the cursor's row: kick, snare and hats.", wait=LISTEN),
        S("Start", "Stop."),
    ]),
    Section("7. Swing", """
Straight 16ths sound stiff. A **groove** sets how long each step lasts, in ticks: `06 06` is even, `07 05` makes every second 16th a little late. That's swing.
""", [
        S("RB+Right", "Chain `02`."),
        S("RB+Right", "Phrase `02`."),
        S("RB+Up", "The **Groove** screen, above Phrase. The cursor is on the first `06`.", expect="Groove"),
        S("A+Right", "`07`."),
        S("Down", "The second `06`."),
        S("A+Left", "`05`: `07 05`, swing.", expect="swing"),
        S("Start", "The hats now shuffle.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Down", "Back on phrase `02`."),
        S("RB+Left", "Chain `02`."),
        S("RB+Left", "Song."),
    ]),
    # --------------------------------------------------------------- harmony
    Section("8. Bass", """
Track 4: a rolling bass, a note on every 8th: `C C C(high) C C C C(high) G`. Just the root, its octave and its 5th, so it fits every chord once the chain moves it around.
""", [
        S("Right", "Track 4, row `00`."),
        S("A", "Chain `02` goes in."),
        S("A", "A new chain `03`.", expect="New chain 03"),
        S("RB+Right", "Chain `03`, cursor on row `3`."),
        S("Up x3", "Row `0`."),
        S("A", "Phrase `02` goes in."),
        S("A", "A new phrase `03`.", expect="New phrase 03"),
        S("RB+Right", "Phrase `03`. The cursor is on step `F`, in the command column, where you left the hats."),
        S("Left x2", "The note column."),
        S("Up x15", "Step `0`."),
        S("A", "`C 3 I02`, a copy of the hat."),
        S("Right", "The cursor is on `I02`."),
        S("A+Right x3", "`I05`, the **BASS**: a saw and a sub, tuned two octaves down.", expect="BASS"),
        S("Left", "Back on the note."),
        S("Down x2", "Step `2`."),
        S("A", "`C 3`."),
        S("Down x2", "Step `4`."),
        S("A", "`C 3`."),
        S("A+Up", "`C 4`: **A + Up** moves a note up an octave."),
        S("Down x2", "Step `6`."),
        S("A", "`C 4`: a new note copies the last one, edit included."),
        S("A+Down", "`C 3`: an octave down."),
        S("Down x2", "Step `8`."),
        S("A", "`C 3`."),
        S("Down x2", "Step `A`."),
        S("A", "`C 3`."),
        S("Down x2", "Step `C`."),
        S("A", "`C 3`."),
        S("A+Up", "`C 4`."),
        S("Down x2", "Step `E`."),
        S("A", "`C 4`."),
        S("A+Left x3", "`G 3`. **A + Left/Right** walks through the notes of C minor: `A#3`, `G#3`, `G 3`."),
        S("Start", "The bass line.", wait=LISTEN),
        S("Start", "Stop."),
    ]),
    Section("9. Try a sound, then undo", """
Every synth has presets to flip through. Try one, and if you don't like it, **B + Select** undoes it (**LB + Select** redoes). Undo works for every change, on every screen.
""", [
        S("RB+Right", "Instrument `I05`, the BASS, cursor on `preset bass`.", expect="BASS"),
        S("A+Right", "`preset subbass`: a clean sine."),
        S("Start", "Deep, but too soft for this song.", wait=LISTEN),
        S("Start", "Stop."),
        S("B+Select", "Undo: `preset bass` is back, with all its settings.", expect="bass"),
        S("RB+Left", "Back on phrase `03`."),
        S("RB+Left", "Back on chain `03`."),
        *rows_of("03", "03"),
    ]),
    Section("10. Four chords from one bar", """
The second column of a chain **transposes** its row: every note of that bar moves up or down, in semitones. So one bar of bass plays four chords:

| Row | Transpose | Chord |
| --- | --- | --- |
| 0 | `00` | C minor |
| 1 | `FC` (4 down) | A♭ major |
| 2 | `03` (3 up) | E♭ major |
| 3 | `FE` (2 down) | B♭ major |

That's the song's chord progression. The pad and the keys use the same trick.
""", [
        *transposes(""),
        S("Start", "**Start** on a chain loops this track's chain: the bass walks through the four chords.", wait=2 * LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("11. The pad: a whole chord from one note", """
Track 5 gets the big chords. The **HyperSynth** engine plays a six-note chord from every note, and with its `scale` switch on, every chord stays in C minor whatever the transpose: a C makes C minor 9, an A♭ makes A♭ major 9.

Two more things make it sit in the mix: a **MOD** slot that ducks it every time the kick hits (the "pumping" of house music), and its own **EQ** taking out the low end, which belongs to the kick and bass.
""", [
        S("Right", "Track 5, row `00`."),
        S("A", "Chain `03` goes in."),
        S("A", "A new chain `04`.", expect="New chain 04"),
        S("RB+Right", "Chain `04`. The cursor is where you left chain `03`: row `1`, on the transpose."),
        S("Left", "The phrase column."),
        S("Up", "Row `0`."),
        S("A", "Phrase `03` goes in."),
        S("A", "A new phrase `04`.", expect="New phrase 04"),
        S("RB+Right", "Phrase `04`, cursor on step `E`."),
        S("Up x14", "Step `0`."),
        S("A", "`G 3 I05`, a copy of the last bass note."),
        S("A+Left x4", "`C 3`."),
        S("Right", "The cursor is on `I05`."),
        S("A+Right x2", "`I07`, the **PAD**.", expect="PAD"),
        S("Left", "Back on the note. One note for the whole bar: it holds until the next one."),
        S("RB+Right", "Instrument `I07`, the PAD.", expect="PAD"),
        S("Up", "The cursor is on `engine synth`."),
        S("A+Right x2", "`engine hyper`: the HyperSynth, starting from its `hyper init` sound.", expect="hyper"),
        S("Down", "The cursor is on `preset hyper init`."),
        S("A+Right", "`preset hyper pad`: slow, wide and warm.", expect="hyper pad"),
        S("Down", "The cursor is on `chord min9`. The two rows under it are the six notes it plays."),
        S("A+Left", "`chord maj9`."),
        S("Down x7", "The cursor is on `scale off`."),
        S("A+Right", "`scale on`."),
        S("Start", "One lush chord.", wait=LISTEN),
        S("Start", "Stop."),
        S("LB+Right x4", "Page 5, **MOD**: four modulation slots, all off.", expect="MOD"),
        S("Down", "The cursor is on slot 1's `type off`."),
        S("A+Right x5", "`type trig`: an envelope fired by the notes of another track, `source track 1`: the kick.",
          expect="trig"),
        S("Down", "The cursor is on `dest cutoff`, what it moves."),
        S("A+Left", "`dest volume`."),
        S("Down", "The cursor is on `amount +64`."),
        S("A+Down x8", "A minus amount: every kick pushes the pad's volume down, and it swells back up."),
        S("LB+Right x2", "Page 7, **EQ**: the pad's own three-band EQ.", expect="EQ"),
        S("A+Down x3", "`l.gain 50`: the lows cut, so the kick and bass have them to themselves."),
        S("LB+Left x6", "Back on page 1."),
        S("RB+Left", "Back on phrase `04`."),
        S("RB+Left", "Back on chain `04`."),
        *rows_of("04", "04"),
        *transposes("The same chords as the bass."),
        S("RB+Left", "Back on the Song screen."),
        S("Start", "Drums, bass and pumping chords.", wait=2 * LISTEN),
        S("Start", "Stop."),
    ]),
    Section("12. FM keys: one note, a chord stab", """
Track 6: an FM electric piano stabbing on the off-beats. The `CHRD` command turns one note into a chord: each digit of its value adds a note that many semitones up. `007E` adds 7 (the 5th) and `E` = 14 (the 9th): an open sound that fits all four chords.
""", [
        S("Right", "Track 6, row `00`."),
        S("A", "Chain `04` goes in."),
        S("A", "A new chain `05`.", expect="New chain 05"),
        S("RB+Right", "Chain `05`, cursor on row `1`'s transpose."),
        S("Left", "The phrase column."),
        S("Up", "Row `0`."),
        S("A", "Phrase `04` goes in."),
        S("A", "A new phrase `05`.", expect="New phrase 05"),
        S("RB+Right", "Phrase `05`, cursor on step `0`."),
        S("Down x2", "Step `2`."),
        S("A", "`C 3 I07`."),
        S("Right", "The cursor is on `I07`."),
        S("A+Right x2", "`I09`, the **KEYS**.", expect="KEYS"),
        S("Right", "The command column."),
        S("Select", "The command list. It always opens on `ARPG`, top left."),
        S("Right x2", "`CHRD`: \"note picks chord\"."),
        S("A", "`CHRD 0000`.", expect="CHRD"),
        S("Right", "The value. The second digit is lit: the last one you edited."),
        S("A+Up x7", "`0070`."),
        S("A+Right", "Holding **A**, **Right** lights the last digit."),
        S("A+Up x14", "`007E`: after `9` come `A` to `F`.", expect="007E"),
        S("Left x3", "Back on the note of step `2`."),
        S("B+LB", "A selection on the note..."),
        S("Right x3", "...grown over the instrument, command and value."),
        S("B", "Copied.", expect="Copied"),
        S("Down x4", "Step `6`."),
        S("A+LB", "Pasted: a second stab. The cursor moves on to step `7`.", expect="CHRD"),
        S("Down x3", "Step `A`."),
        S("A+LB", "A third."),
        S("Down x3", "Step `E`."),
        S("A+LB", "And a fourth."),
        S("RB+Right", "Instrument `I09`, the KEYS.", expect="KEYS"),
        S("Up", "The cursor is on `engine synth`."),
        S("A+Right", "`engine fm4`: four-operator FM, starting from `fm init`.", expect="fm4"),
        S("Down", "The cursor is on `preset fm init`."),
        S("A+Right", "`preset epiano`: a classic FM electric piano.", expect="epiano"),
        S("Start", "The stabs.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on phrase `05`."),
        S("RB+Left", "Back on chain `05`."),
        *rows_of("05", "05"),
        *transposes(""),
        S("RB+Left", "Back on the Song screen."),
    ]),
    # ------------------------------------------------------------------ hook
    Section("13. The hook", """
Track 7: the lead. First let the app write a random melody, just to hear what it does, and undo it. Then enter the real hook, three bars (bars 1 and 3 are the same):

| Step | 0 | 3 | 6 | A | C | E |
| --- | --- | --- | --- | --- | --- | --- |
| Phrase `06` (bars 1, 3) | `G 4` | `G 4` | `A#4` | `G 4` | `F 4` | `D#4` |
| Phrase `07` (bar 2) | `D#4` | `D#4` | `G 4` | `F 4` | `D#4` | `C 4` |
| Phrase `08` (bar 4) | `F 4` | `F 4` | `A#4` | `C 5` | `A#4` | `F 4` |

The same rhythm every bar with a new shape each time: that's what makes a hook stick.
""", [
        S("Right", "Track 7, row `00`."),
        S("A", "Chain `05` goes in."),
        S("A", "A new chain `06`.", expect="New chain 06"),
        S("RB+Right", "Chain `06`, cursor on row `1`'s transpose."),
        S("Left", "The phrase column."),
        S("Up", "Row `0`."),
        S("A", "Phrase `05` goes in."),
        S("A", "A new phrase `06`.", expect="New phrase 06"),
        S("RB+Right", "Phrase `06`, cursor on step `F`."),
        S("Up x15", "Step `0`."),
        S("B+LB", "A selection..."),
        S("Down x15", "...over the whole bar."),
        S("LB+Right", "**LB + Right**: random notes in C minor on random steps."),
        S("Start", "A melody out of nowhere. Press **LB + Right** again for another.", wait=LISTEN),
        S("Start", "Stop."),
        S("B+Select", "Undo: the phrase is empty again. You'll write this one yourself."),
        S("B", "**B** ends the selection."),
        S("A", "`C 3 I09`."),
        S("Right", "The cursor is on `I09`."),
        S("A+Left x3", "`I06`, the **LEAD**.", expect="LEAD"),
        S("Left", "Back on the note."),
        S("A+Up", "`C 4`."),
        S("A+Right x4", "`G 4`, through `D 4`, `D#4` and `F 4`."),
        S("Down x3", "Step `3`."),
        S("A", "`G 4`."),
        S("Down x3", "Step `6`."),
        S("A", "`G 4`."),
        S("A+Right x2", "`A#4`."),
        S("Down x4", "Step `A`."),
        S("A", "`A#4`."),
        S("A+Left x2", "`G 4`."),
        S("Down x2", "Step `C`."),
        S("A", "`G 4`."),
        S("A+Left", "`F 4`."),
        S("Down x2", "Step `E`."),
        S("A", "`F 4`."),
        S("A+Left", "`D#4`."),
        S("RB+Right", "Instrument `I06`, the LEAD.", expect="LEAD"),
        S("Up", "The cursor is on `engine synth`."),
        S("A+Right x2", "`engine hyper`.", expect="hyper"),
        S("Down", "The cursor is on `preset hyper init`."),
        S("A+Right x2", "`preset trance lead`: detuned saws, with echo and reverb already dialled in.",
          expect="trance lead"),
        S("Start", "Bar 1 of the hook.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on phrase `06`."),
        S("RB+Left", "Back on chain `06`."),
        S("Down x2", "Row `2`: bar 3 is bar 1 again."),
        S("A", "Phrase `06`."),
        S("Up", "Row `1`."),
        S("A", "Phrase `06`..."),
        S("A", "...and **A** again: a new phrase `07` for bar 2.", expect="New phrase 07"),
        S("RB+Right", "Phrase `07`, cursor on step `E`."),
        S("Up x14", "Step `0`."),
        S("A", "`D#4 I06`: the last note you entered, and its instrument."),
        S("Down x3", "Step `3`."),
        S("A", "`D#4`."),
        S("Down x3", "Step `6`."),
        S("A", "`D#4`."),
        S("A+Right x2", "`G 4`."),
        S("Down x4", "Step `A`."),
        S("A", "`G 4`."),
        S("A+Left", "`F 4`."),
        S("Down x2", "Step `C`."),
        S("A", "`F 4`."),
        S("A+Left", "`D#4`."),
        S("Down x2", "Step `E`."),
        S("A", "`D#4`."),
        S("A+Left x2", "`C 4`."),
        S("RB+Left", "Back on chain `06`."),
        S("Down x2", "Row `3`."),
        S("A", "Phrase `07`..."),
        S("A", "...and a new phrase `08` for bar 4.", expect="New phrase 08"),
        S("RB+Right", "Phrase `08`, cursor on step `E`."),
        S("Up x14", "Step `0`."),
        S("A", "`C 4 I06`."),
        S("A+Right x3", "`F 4`."),
        S("Down x3", "Step `3`."),
        S("A", "`F 4`."),
        S("Down x3", "Step `6`."),
        S("A", "`F 4`."),
        S("A+Right x3", "`A#4`."),
        S("Down x4", "Step `A`."),
        S("A", "`A#4`."),
        S("A+Right", "`C 5`: the top of the hook."),
        S("Down x2", "Step `C`."),
        S("A", "`C 5`."),
        S("A+Left", "`A#4`."),
        S("Down x2", "Step `E`."),
        S("A", "`A#4`."),
        S("A+Left x3", "`F 4`."),
        S("RB+Left", "Back on chain `06`."),
        S("Start", "The whole hook, four bars.", wait=3 * LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on the Song screen."),
        S("Start", "Everything together: this is the drop.", wait=3 * LISTEN),
        S("Start", "Stop."),
    ]),
    # ----------------------------------------------------------- arrangement
    Section("14. Copy the drop", """
Row `00` is the whole drop. The arrangement is that row, copied and changed: parts drop out, fills lead into each drop. First, copy it to rows `01` to `03`.
""", [
        S("Left x6", "Track 1."),
        S("B+LB", "A selection on the cell..."),
        S("Right x7", "...over the whole row."),
        S("B", "Copied.", expect="copied"),
        S("Down", "Row `01`."),
        S("A+LB", "Pasted: row `01` plays the drop too. The cursor moves on to the next row."),
        S("A+LB", "Row `02`."),
        S("A+LB", "Row `03`."),
    ]),
    Section("15. Silence: a rest chain", """
A track only goes quiet where it plays something silent: a chain whose phrase says `KILL` (stop the note). Chain `07` is that rest. You'll put it everywhere a part should drop out, and every track needs something on every row: a track that reaches an empty cell stops for the rest of the song.
""", [
        S("Up x4", "Row `00`: it becomes the intro, just hats, pad and keys."),
        S("Right x7", "Track 8, still empty."),
        S("A", "The last chain goes in..."),
        S("A", "...and **A** again: a new chain `07`.", expect="New chain 07"),
        S("RB+Right", "Chain `07`, empty."),
        S("A", "Phrase `08` goes in."),
        S("A", "A new phrase `09`.", expect="New phrase 09"),
        S("RB+Right", "Phrase `09`, cursor on step `E`."),
        S("Up x14", "Step `0`."),
        S("Right x2", "The command column."),
        S("Select", "The command list."),
        S("Down x2", "Down two rows..."),
        S("Right x4", "...`KILL`: stop the note."),
        S("A", "`KILL 0000`: whatever this track plays stops right here.", expect="KILL"),
        S("RB+Left", "Back on chain `07`."),
        *rows_of("07", "09"),
        S("RB+Left", "Back on the Song screen."),
        S("Down", "Row `01`, track 8."),
        S("A", "`07`: rest."),
        S("Down", "Row `02`."),
        S("A", "`07`."),
        S("Down", "Row `03`."),
        S("A", "`07`."),
        S("Up x3", "Row `00`."),
        S("Left x7", "Track 1, the kick."),
        S("A+Right x7", "`07`: **A + Right** counts the chain number up. No kick in the intro."),
        S("Right", "Track 2."),
        S("A+Right x6", "`07`."),
        S("Right x2", "Track 4."),
        S("A+Right x4", "`07`."),
        S("Right x3", "Track 7."),
        S("A+Right", "`07`."),
        S("Down", "Row `01`, track 7."),
        S("A+Right", "`07`: no lead before the first drop."),
    ]),
    Section("16. A snare roll", """
Row `01` is the build before the drop. Its snare plays three bars of snare, then a roll: a new chain `08`. The roll is one note with two commands: `ROLL 0093` strikes it again every 3 ticks, each hit 8 louder, starting from `VOLM 0020`, quiet.
""", [
        S("Left x5", "Track 2."),
        S("A+Right x7", "`08`: a chain nobody uses yet, so it's new and empty."),
        S("RB+Right", "Chain `08`, cursor on row `3`."),
        S("Up x3", "Row `0`."),
        S("A+Left x8", "**A** puts the last phrase (`09`) in, **Left** counts it down to `01`, the snare."),
        S("Down", "Row `1`."),
        S("A", "Phrase `01`."),
        S("Down", "Row `2`."),
        S("A", "Phrase `01`."),
        S("Down", "Row `3`."),
        S("A", "Phrase `01`..."),
        S("A", "...and a new phrase `0A` for the roll.", expect="New phrase 0A"),
        S("RB+Right", "Phrase `0A`, cursor on step `0`, in the command column."),
        S("Left x2", "The note column."),
        S("A", "`F 4 I06`, the last note you entered."),
        S("A+Down", "`F 3`."),
        S("A+Left x3", "`C 3`."),
        S("Right", "The instrument."),
        S("A+Left x5", "`I01`, the SNARE.", expect="SNARE"),
        S("Right", "The command column."),
        S("Select", "The command list."),
        S("Down x5", "Down five rows..."),
        S("Right x2", "...`ROLL`."),
        S("A", "`ROLL 0000`.", expect="ROLL"),
        S("Right", "The value. The last digit is lit."),
        S("A+Up x3", "`0003`: a hit every 3 ticks."),
        S("A+Left", "The next digit."),
        S("A+Up x9", "`0093`: `9` = each hit 8 louder."),
        S("Right", "The second command column."),
        S("Select", "The command list."),
        S("Down x7", "The bottom row..."),
        S("Right x4", "...`VOLM`, volume."),
        S("A", "`VOLM 0000`.", expect="VOLM"),
        S("Right", "The value, its second digit lit."),
        S("A+Up x2", "`0020`: the roll starts quiet."),
        S("Start", "The roll swells.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on chain `08`."),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("17. A sample: the reversed cymbal", """
Track 8 plays effects. The first is a crash cymbal from the sample packs, played backwards: it swells up into the drop. Instrument `03` (the kit's open hat, unused) becomes a sample.

The crash is 2.5 seconds long and a bar is 1.9. Played at `F 3`, five semitones up, it's faster and lasts exactly one bar.
""", [
        S("Right x6", "Track 8, row `01`."),
        S("A+Right x2", "`09`, a new chain."),
        S("RB+Right", "Chain `09`, cursor on row `3`."),
        S("Up x3", "Row `0`."),
        S("A+Left", "The last phrase (`0A`) counted down to `09`, the rest."),
        S("Down", "Row `1`."),
        S("A", "Phrase `09`."),
        S("Down", "Row `2`."),
        S("A", "Phrase `09`."),
        S("Down", "Row `3`."),
        S("A", "Phrase `09`..."),
        S("A", "...and a new phrase `0B`.", expect="New phrase 0B"),
        S("RB+Right", "Phrase `0B`, cursor on step `0`, second command column."),
        S("Left x5", "The note column."),
        S("A", "`C 3 I01`."),
        S("A+Right x3", "`F 3`."),
        S("Right", "The instrument."),
        S("A+Right x2", "`I03`, the OPENHAT."),
        S("RB+Right", "Instrument `I03`."),
        S("Up x2", "The cursor is on `type`."),
        S("A+Left", "`type sample`: an empty sampler.", expect="SAMPLE"),
        S("Down", "The cursor is on `sample none`."),
        S("Select", "**Select** opens the sample browser: `Applications/Samples`, one folder per pack.", wait=300),
        S("Down x6", "`[drums-909]`."),
        S("A", "Inside the folder.", wait=200),
        S("Down x2", "`crash.wav`. **Listen** would play it."),
        S("Right", "`Import` is lit."),
        S("A", "Imported: the crash is copied into your song.", wait=500),
        S("Right x2", "`Exit` is lit."),
        S("A", "Back on the instrument: the waveform of the crash, loudest at the start.", wait=300, expect="crash"),
        S("Down", "The cursor is on `play forward`."),
        S("A+Right", "`play reverse`: end to start."),
        S("Start", "The phrase: a cymbal swelling up, one bar long.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on phrase `0B`."),
        S("RB+Left", "Back on chain `09`."),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("18. Resampling: the reversed pad", """
**Render to sample** records a bar of your song into a new sample. You'll record one bar of the pad and play it backwards: a swell that pulls you out of the break.
""", [
        S("Up", "Row `00`."),
        S("Left x3", "Track 5, the pad."),
        S("RB+Right", "Chain `04`, cursor on row `3`."),
        S("Up x3", "Row `0`."),
        S("RB+Right", "Phrase `04`."),
        S("LB+Start", "**LB + Start**: the bar plays once and is recorded.", wait=300),
        S("", "Done: `rs_01.wav` is saved in instrument `10`.", wait=2600, expect="saved"),
        S("A", "Instrument `10`: the recorded bar. The cursor is on `play forward`.", wait=300),
        S("A+Right", "`play reverse`."),
        S("RB+Left", "Back on phrase `04`."),
        S("RB+Left", "Back on chain `04`."),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("19. The break", """
Row `04` is the break: just the pad and the lead, and the reversed pad at the end. It starts as a copy of the intro.
""", [
        S("Left x4", "Track 1."),
        S("B+LB", "Selection..."),
        S("Right x7", "...the whole intro row."),
        S("B", "Copied."),
        S("Down x4", "Row `04`."),
        S("A+LB", "Pasted."),
        S("Up", "Back on row `04`."),
        S("Right x2", "Track 3."),
        S("A+Right x5", "`07`: no hats in the break."),
        S("Right x3", "Track 6."),
        S("A+Right x2", "`07`: no keys."),
        S("Right", "Track 7."),
        S("A+Left", "`06`: the hook comes in."),
        S("Right", "Track 8."),
        S("A+Right x3", "`0A`, a new chain."),
        S("RB+Right", "Chain `0A`, cursor on row `0`."),
        S("A+Left x2", "The last phrase (`0B`) counted down to `09`, the rest."),
        S("Down", "Row `1`."),
        S("A", "Phrase `09`."),
        S("Down", "Row `2`."),
        S("A", "Phrase `09`."),
        S("Down", "Row `3`."),
        S("A", "Phrase `09`..."),
        S("A", "...and a new phrase `0C`.", expect="New phrase 0C"),
        S("RB+Right", "Phrase `0C`, cursor on step `0`, on the instrument column."),
        S("Left", "The note column."),
        S("A", "`F 3 I03`."),
        S("A+Left x3", "`C 3`."),
        S("Right", "The instrument."),
        S("A+Up", "**A + Up**: 16 up, `I13`."),
        S("A+Left x3", "`I10`: the recorded pad.", expect="I10"),
        S("Start", "The pad, backwards.", wait=LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "Back on chain `0A`."),
        S("RB+Left", "Back on the Song screen."),
    ]),
    Section("20. Build, drop, outro", """
Three more rows: `05` is another build (like `01`, with the lead), `06` the last drop (like `02`), `07` the outro (like the intro).
""", [
        S("Up x3", "Row `01`."),
        S("Left x7", "Track 1."),
        S("B+LB", "Selection..."),
        S("Right x7", "...row `01`."),
        S("B", "Copied."),
        S("Down x4", "Row `05`."),
        S("A+LB", "Pasted."),
        S("Up", "Back on row `05`."),
        S("Right x3", "Track 4."),
        S("A+Right x4", "`07`: no bass while it builds."),
        S("Right x3", "Track 7."),
        S("A+Left", "`06`: the lead plays this build."),
        S("Up x3", "Row `02`."),
        S("Left x6", "Track 1."),
        S("B+LB", "Selection..."),
        S("Right x7", "...the drop."),
        S("B", "Copied."),
        S("Down x4", "Row `06`."),
        S("A+LB", "Pasted: the last drop."),
        S("Up x7", "Row `00`."),
        S("B+LB", "Selection..."),
        S("Right x7", "...the intro."),
        S("B", "Copied."),
        S("Down x7", "Row `07`."),
        S("A+LB", "Pasted: the outro. After it the song ends."),
    ]),
    Section("21. Bookmarks", """
Bookmarks mark where the sections start, so you can jump around a long song.
""", [
        S("Up x8", "Row `00`, the intro."),
        S("A+Select", "**A + Select** bookmarks the row."),
        S("Down", "Row `01`, the build."),
        S("A+Select", "Bookmarked."),
        S("Down", "Row `02`, the drop."),
        S("A+Select", "Bookmarked."),
        S("Down x2", "Row `04`, the break."),
        S("A+Select", "Bookmarked."),
        S("Down x2", "Row `06`, the last drop."),
        S("A+Select", "Bookmarked."),
        S("LB+Up", "**LB + Up** jumps to the previous bookmark: row `04`."),
        S("LB+Up x2", "Row `01`."),
        S("Up", "Row `00`."),
        S("Start", "The whole song from the top.", wait=4 * LISTEN),
        S("Start", "Stop."),
    ]),
    # -------------------------------------------------------------------- mix
    Section("22. Mix", """
The **Mixer** sets each track's level. The kick should hit hardest; the bass sits a little under it.
""", [
        S("RB+Down", "The **Mixer**: a fader per track, then the effect returns and the master.", expect="Mixer"),
        S("A+Up x4", "Track 1, the kick, at `FF`: its loudest."),
        S("Right x3", "Track 4, the bass."),
        S("A+Down x3", "`90`: a bit down."),
    ]),
    Section("23. Effects, EQ and limiter", """
The **FX** screen sets up the shared chorus, echo and reverb the instruments send to. **EQ** shapes the whole mix, **Limit** makes it loud without clipping.
""", [
        S("RB+Down", "The **FX** screen.", expect="REVERB"),
        S("Down x4", "The cursor is on the reverb's `size 90`."),
        S("A+Up x2", "`size B0`: a bigger room, a longer tail."),
        S("RB+Right", "The **EQ** screen: low, mid and high for the whole mix.", expect="LOW"),
        S("Down x4", "The cursor is on the high `gain 80`."),
        S("A+Up", "`gain 90`: a little air on top."),
        S("RB+Right", "The **Limit** screen, cursor on `drive 00` (off).", expect="LIMITER"),
        S("A+Up x4", "`drive 40`: the limiter pushes the mix up and catches every peak."),
        S("Start", "Listen: `GR` shows how much it's catching.", wait=2 * LISTEN),
        S("Start", "Stop."),
        S("RB+Left", "EQ."),
        S("RB+Left", "FX."),
        S("RB+Up", "Mixer."),
        S("RB+Up", "Song."),
    ]),
    # ------------------------------------------------------------------ finish
    Section("24. Save", "", [
        S("RB+Up", "Project."),
        S("Up x6", "The cursor is on `Save Song`."),
        S("A", "Saved.", wait=500),
        S("RB+Down", "Song."),
    ]),
    Section("25. Play it live", """
**Live mode** turns the Song screen into a launcher: cue rows and cells while the song plays, M8 style. Nothing you do here changes the song.
""", [
        S("Select", "**Select**: Live mode. The title says `Live`.", expect="Live"),
        S("LB+Start", "**LB + Start** launches row `00`: the intro loops.", wait=2 * LISTEN),
        S("LB+Down", "**LB + Down**: the next bookmark, the build."),
        S("LB+Start", "Cued: it starts when the intro's chains end, and loops.", wait=3 * LISTEN),
        S("LB+Down", "The drop."),
        S("LB+Start", "Cued. Drop it!", wait=3 * LISTEN),
        S("B+Start", "**B + Start** stops everything."),
        S("Select", "Back to Song mode.", expect="Song"),
        S("LB+Up x2", "Row `00`."),
    ]),
    Section("26. Export a WAV", """
Finally, record the song into a WAV file you can share. It plays in real time while it records.
""", [
        S("RB+Up", "Project."),
        S("Down x4", "The cursor is on `Render: Off`."),
        S("A+Right", "`Render: Stereo`: the next play records `mixdown.wav` in the song's folder."),
        S("RB+Down", "Song."),
        S("Start", "Recording... let it play to the end (about a minute).", wait=3 * LISTEN),
        S("Start", "Stopped: the file is closed. (Here it's cut short; yours has the whole song.)"),
        S("RB+Up", "Project."),
        S("A+Left", "`Render: Off` again, for normal playing."),
        S("RB+Down", "Song. **Afterglow** is done: GLOW is your song now."),
    ]),
]
