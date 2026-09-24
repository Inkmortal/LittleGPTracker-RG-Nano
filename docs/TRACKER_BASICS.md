# Tracker Basics: Make Music on the RG Nano From Zero

This guide assumes you have never used a tracker and know very little music theory. Follow it top to bottom once with the device in your hands; after that, use it as a cheat sheet.

On any screen, **RB + Select** opens the helper. Press **Down** to flip through its three pages: a map of the screens, the buttons for this screen, and a short "HOW TO" for this screen.

## 1. What a tracker is

A tracker is a spreadsheet that plays music. Time goes **down** the screen, one row at a time. Instead of dragging notes on a piano roll, you type them into rows.

There are four layers, from biggest to smallest:

| Layer | What it is | Think of it as |
| --- | --- | --- |
| **Song** | 8 columns (tracks) × many rows. Each cell names a chain. | The whole arrangement |
| **Chain** | A list of phrases for one track, played one after another. | A section, e.g. "4 bars of drums" |
| **Phrase** | 16 steps. One step is a 16th note, so one phrase = one bar. | One bar of notes |
| **Instrument** | The sound a note uses: a synth or a sample. | The patch / preset |

Move between them by holding **RB** and pressing a direction:

```
            Project            Groove
               |                  |
   Song  -- RB+Right -->  Chain -- RB+Right -->  Phrase -- RB+Right --> Instrument
   (RB+Left goes back the other way)
```

Two more things you will see everywhere:

- **Hex numbers.** Values go `00 01 … 09 0A 0B … 0F 10 … FF`. `80` is the middle of `00`–`FF`, `40` is a quarter, `C0` is three quarters. You never need to convert them — just nudge until it sounds right.
- **Notes** look like `C 3`, `D#3`, `A 2`. The letter is the note, the number is the octave. `A 3` is 440 Hz (concert A).

## 2. The sounds you already have

Every new project starts with 16 ready-made synth instruments, so the first note you type makes a sound:

| Instrument | Sound | Instrument | Sound |
| --- | --- | --- | --- |
| `00` | KICK | `08` | PLUCK (arps) |
| `01` | SNARE | `09` | KEYS (e-piano) |
| `02` | HAT (closed hi-hat) | `0A` | BELL |
| `03` | OPENHAT | `0B` | ACID (squelchy bass) |
| `04` | CLAP | `0C` | SUBBASS |
| `05` | BASS | `0D` | CHIP (8-bit) |
| `06` | LEAD | `0E` | TOM |
| `07` | PAD (chords) | `0F` | PERC (woodblock) |

Drums play at a sensible pitch when you type `C 3`. Basses are pre-tuned two octaves down, so `C 3` on the bass already sounds deep.

## 3. Your first beat (5 minutes)

1. **New song.** On the **Your Songs** start screen press **Right** to highlight `New`, then **A**. The name box opens with a free random name and `DONE` highlighted: press **A** again. (Type your own name with the letter grid if you like; **B** erases, **B** on an empty name goes back.) You are on the **Song** screen. The `Help` button on the start screen has a short tour of all of this.
2. **Make a chain for track 1.** The cursor is on row `00`, column 1 (`--`). Press **A**. It becomes `00` — chain 00 now plays on track 1.
3. **Open it.** **RB + Right**. You are in **Chain 00**.
4. **Make a phrase.** Press **A** on the first `--`. It becomes `00`.
5. **Open it.** **RB + Right**. You are in **Phrase 00**: 16 empty rows.
6. **Kick drum.** On row `00` press **A**: a `C 3` note with instrument `I00` (KICK) appears. Move down to row `04` and press **A** again. Do it on rows `08` and `0C`. That is "four on the floor": a kick on every beat.
7. **Listen.** Press **Start**. The bar loops. Press **Start** again to stop.

That's a beat. Now add the snare and hats on their own tracks:

8. **RB + Left** twice to get back to **Song**. Move **Right** to column 2 and press **A**. The screen says `Reused 00`: pressing A on an empty cell repeats the last chain (handy for repeating a part). Press **A** again for a brand-new one: `New chain 01`. **RB + Right**, then **A A** again for `New phrase 01`, and **RB + Right**.
9. In the new phrase, put notes on rows `04` and `0C` (beats 2 and 4). Each new note copies the last instrument, so move to the instrument column (`I00`) and hold **A + Right** until it says `I01` (SNARE). New notes after that use `01`.
10. Do the same on column 3 with instrument `02` (HAT) on rows `00 02 04 06 08 0A 0C 0E` (every 8th note).
11. Go back to **Song** and press **Start** there. All three tracks play together because their chains sit on the same Song row.

**Remember:** one **A** on an empty cell = reuse the last chain/phrase, **A A** = a new empty one.

**Why it works:** Song rows are played top to bottom. Everything on the same row plays at the same time, one chain per track.

## 4. A bassline

1. On **Song** column 4, make a chain and phrase as before, and set the instrument to `05` (BASS).
2. Put a note on row `00`, then press **A + Up/Down** to change the octave and **A + Left/Right** to change the note. Try `A 2` on rows `00`, `06`, `0A`, then `G 2` on row `0E`.
3. The bass keeps ringing until the next note. To stop a note early, put the command `KILL 0000` on a row (see section 7).

**Tip:** If you set the Project `Key:` to a key (for example `A`) and `Scale:` to `Aeolian mode (minor)`, then **A + Left/Right** on a note only walks through notes that fit. Hold **LB** instead of A to escape the scale for one note.

## 5. Chords in one track

Each track plays one note at a time, but the synth can play a whole chord from one note with the `CHRD` command.

**How it works — two parts:**

- **The note you type picks WHICH chord.** Type `A 2` and you get an A chord.
- **The CHRD value picks WHAT KIND.** `0047` = major (happy), `0037` = minor (sad).

So `A 2` + `CHRD 0037` = **A minor**, and `F 2` + `CHRD 0047` = **F major**. The CHRD value on its own is never "A" or "F" — it's only major or minor.

**What the digits actually do:** each digit says "also play the note this many steps up from mine". `0037` on `A 2`:

```text
A                               <- your note
A  A# B  C                      <- 3 steps up:  C
A  A# B  C  C# D  D# E          <- 7 steps up:  E
                                   plays A + C + E = A minor
```

Count every key, black ones included (A → A# → B → C is 3 steps). Past 9 the digits are hex letters: `A` = 10, `B` = 11.

| Type this note | + CHRD `0037` plays | + CHRD `0047` plays |
| --- | --- | --- |
| `A 2` | A C E = **A minor** | A C# E = A major |
| `C 3` | C D# G = C minor | C E G = **C major** |
| `D 2` | D F A = **D minor** | D F# A = D major |
| `E 2` | E G B = **E minor** | E G# B = E major |
| `F 2` | F G# C = F minor | F A C = **F major** |
| `G 2` | G A# D = G minor | G B D = **G major** |

The **bold** ones are the chords that belong to A minor / C major — use those and it always sounds right.

More kinds, same idea (the note still picks which chord):

| CHRD | Kind | Sounds |
| --- | --- | --- |
| `0047` | major | happy, bright |
| `0037` | minor | sad, serious |
| `047B` | major 7 | dreamy, lo-fi |
| `037A` | minor 7 | smooth, jazzy |
| `047A` | dominant 7 | bluesy, wants to move on |
| `0057` | sus4 | open, unresolved |
| `0027` | sus2 | airy |
| `0007` | power chord | rock, neutral |

**Try it:** make a track with instrument `07` (PAD). Put `A 2` on row `00`, move to the first command column (`----`), press **Select**, pick `CHRD`, set it to `0037`. For a whole-bar pad, put one chord at row `00` of each phrase.

## 6. Turning a loop into a song

A **chain** can hold up to 16 phrases (bars). A common pattern:

- Drums: chain = `[beat, beat, beat, beat-with-fill]`
- Chords: chain = `[chord1, chord2, chord3, chord4]` — a 4-bar progression
- Each Song row = one section (intro, verse, chorus...). Keep every chain on a row the **same length** so the tracks stay in sync.

To make a track silent in a section, give it a chain whose phrases are empty (put `KILL 0000` on the first step so the last note doesn't keep ringing). An empty `--` Song cell makes that track jump back and loop instead.

Classic song map (each line = one Song row = 4 bars):

```
intro    pad + arp
verse    drums + bass + pad
verse    + melody
chorus   everything, bigger drums
break    drums drop out
chorus   again
outro    pad + arp fading
```

## 7. The commands you will actually use

Put commands in the two command columns of a phrase (press **Select** on `----` to pick one). The value is 4 hex digits.

| Command | What it does | Example |
| --- | --- | --- |
| `VOLM` | Volume for this note. `00xx` = instant, `yyxx` = fade over time | `VOLM 0050` quiet ghost note |
| `KILL` | Stop the note (after `xx` ticks) | `KILL 0000` |
| `CHRD` | Play a chord (synths) | `CHRD 0037` minor |
| `ARPG` | Arpeggio: cycle the notes fast | `ARPG 0047` chiptune major |
| `RTRG` | Retrigger every `xx` ticks: rolls, stutters | `RTRG 0003` 32nd-note roll |
| `PTCH` | Bend pitch by `xx` semitones | `PTCH 1002` bend up 2 |
| `LEGA` | Slide into the note | `LEGA 10F4` slide up from 12 below |
| `FCUT` | Filter brightness (`yy` speed, `xx` target) | `FCUT 60C0` slow sweep open |
| `DLAY` | Start the note a few ticks late (swing a single note) | `DLAY 0002` |
| `GROV` | Switch groove (swing) | `GROV 0001` |

One step is 6 ticks at the default groove.

## 8. Shaping sounds (the synth screen)

On the Instrument screen press **LB + Left/Right** to switch pages. The picture at the top shows what the knobs do, and the text under it explains the focused knob in plain words with real units (ms, Hz).

| Page | Knobs | Try this |
| --- | --- | --- |
| SOUND | type, preset, wave, shape, sub, noise, fm, ratio, chord, tune | `preset`: **A + Left/Right** to audition every ready sound |
| ENV | attack, decay, sustain, release, pitch env, glide | sustain `00` = pluck, attack `A0` = slow swell |
| FILTER | filter type, cutoff, resonance, env, drive | cutoff down = darker, resonance up = squelch |
| LFO | LFO target, rate, depth, fine tune, table | lfo `pitch`, depth `10` = vibrato |
| MOD | two envelopes/LFOs aimed at volume, cutoff, reso, pitch or pan | `mod1 decay`, `dest1 cutoff` = filter pluck |
| MIX | volume, pan, reverb, delay | reverb `60` on snares and pads |

**Start** plays the sound at `C 3`, **RB + A + Left/Up/Right** plays it low/middle/high, **RB + A + Down** stops. Change `type` to `sample` to play a WAV file instead.

The reverb room size and echo time are shared by all instruments: set them on the **Project** screen (`Reverb`, `Damp`, `Echo` in 16th notes, `Fdbk`).

## 9. Music theory crash course

**Keys and scales.** A key is a home note plus the notes that sound good with it. Beginner-safe choices:

| Scale | Notes (from A) | Feel |
| --- | --- | --- |
| A minor | A B C D E F G | emotional, most electronic music |
| A minor pentatonic | A C D E G | can't play a wrong note; Chinese/wuxia, blues, rock |
| C major | C D E F G A B | happy, pop |

**Chords that fit A minor** (use these roots with the CHRD values above):

`Am (A, 0037)` · `C (C, 0047)` · `Dm (D, 0037)` · `Em (E, 0037)` · `F (F, 0047)` · `G (G, 0047)`

**Progressions that always work** (one chord per bar):

- `Am F C G` — the synthwave / pop anthem loop
- `F G Em Am` — lift into a chorus
- `Am Am F G` — dark and driving
- `Dm7 G7 Cmaj7 Am7` (`037A`, `047A`, `047B`, `037A`) — lo-fi / jazz

**Melody rules of thumb**

- Start and end phrases on a note of the current chord.
- Mostly move by small steps; a big jump sounds great if you step back afterwards.
- Leave space: rests make a melody memorable.
- Repeat a 2-bar idea, then change only its ending.

**Drum patterns** (x = hit, 16 steps):

```
four on the floor   kick  x...x...x...x...   snare ....x.......x...   hats ..x...x...x...x.
boom bap (lo-fi)    kick  x.....x...x.....   snare ....x.......x...   hats x.x.x.x.x.x.x.x.
half time (epic)    kick  x.......x.x.....   snare ........x.......
```

Make hats human with volumes: `VOLM 0060` on every other hat.

## 10. Learn from the demo songs

Open these from the project list (copy the `lgpt_*` folders from `projects/resources/demos` into your `Tracks` folder). Each is a full song built only from the synth kit, and every trick above is used somewhere:

| Project | Style | Look at |
| --- | --- | --- |
| `lgpt_NeonDrive` | synthwave, A minor, 100 BPM | Song rows as sections, CHRD pads, FCUT sweep on the intro arp, snare roll with RTRG in the build |
| `lgpt_JadeSword` | wuxia / donghua, A minor pentatonic, 96 BPM | the same song map as Neon Drive with guzheng, dizi, erhu, taiko and gong built from synths |

The best way to learn: open a demo, go to a phrase you like, change a few notes, and press **Start**.
