# Commands

Commands live in the two command columns of a phrase and the three columns of a table. Each is four letters plus a 4-digit hex value, usually written `aabb`. Put the cursor on a `----` column and press **Select** to pick from the list.

<img src="images/demo-phrase-chords.png" width="300" align="right" alt="A chord command in a phrase">

**Where it goes matters:**

- On the same step as a note, a command shapes that note.
- On a step without a note, it changes whatever is already playing on that track.
- A note **with** an instrument number resets that instrument's ramps. A note **without** one (`I--`) keeps sweeps and fades running.

Timing: 1 step = 6 ticks at the default groove. Ramp speeds (`aa`) count in groups of ticks — bigger `aa` = slower.

<br clear="right">

## The ones you'll use every day

| Command | Value | Does | Try |
| --- | --- | --- | --- |
| `VOLM` | `aabb` | volume to `bb`, ramping over `aa` (`00` = instant) | `VOLM 0050` ghost note · `VOLM 4000` slow fade out |
| `KILL` | `--bb` | stop the note after `bb` ticks | `KILL 0000` cut now · `KILL 0003` half a step |
| `CHRD` | `abcd` | turn the note into a chord: the note picks **which** chord, the value picks **what kind** (each digit = also play the note that many steps up). On a `hyper` synth it replaces the six chord notes (the chord, then again an octave up) | `A 2` + `0037` = A minor · `C 3` + `0047` = C major |
| `ARPG` | `abcd` | cycle root, +a, +b, +c, +d every tick | `ARPG 0047` chiptune major |
| `RTRG` | `aabb` | retrigger every `bb` ticks | `RTRG 0003` 32nd roll · `RTRG 0002` tremolo |
| `PTCH` | `aabb` | bend to `bb` semitones (signed) at speed `aa` | `PTCH 1002` up 2 · `PTCH 10FE` down 2 |
| `LEGA` | `aabb` | slide into the note from `bb` semitones away | `LEGA 10F4` slide up from an octave below |
| `FCUT` | `aabb` | filter cutoff to `bb` at speed `aa` | `FCUT 60C0` open over ~4 bars |
| `FRES` | `aabb` | filter resonance to `bb` at speed `aa` | `FRES 00C0` squelch now |
| `PAN ` | `aabb` | pan to `bb` (`00` left, `7F` centre, `FE` right) | `PAN 2000` sweep left |
| `DLAY` | `--bb` | start the note `bb` ticks late | `DLAY 0003` push a note behind the beat |

## Randomness

Two commands that make every pass a little different, as on the M8:

| Command | Value | Does | Try |
| --- | --- | --- | --- |
| `RAND` | `00bb` | adds a random amount, up to `bb`, to the **other command** on the same step. On its own it moves the **note** up to `bb` semitones, staying in the song's Key/Scale | `VOLM 0060` + `RAND 0060`: velocity between 60 and C0 · `FCUT 0040` + `RAND 0080`: a wandering filter · `RAND 000C` alone: a new melody every loop |
| `CHNC` | `00bb` | the note plays with a chance of `bb`/`FF` (`FF` always, `80` half the time, `00` never) | ghost hats with `CHNC 0060` · a fill that only sometimes happens |

Put them on a step with a note. Both can go on one step (`CHNC` in one column, `RAND` in the other).

## Song-level

| Command | Value | Does |
| --- | --- | --- |
| `TMPO` | `--bb` | set tempo to `bb` (hex BPM) |
| `GROV` | `--bb` | switch this track to groove `bb` |
| `TABL` | `--bb` | start table `bb` on this track |
| `HOP ` | `aabb` | jump to step `bb`, `aa` times (make short or odd-length loops) |
| `STOP` | `----` | stop the song |

## Sound shaping

| Command | Value | Does |
| --- | --- | --- |
| `FLTR` | `aabb` | set cutoff `aa` and resonance `bb` instantly |
| `PFIN` | `aabb` | fine pitch toward `bb` |
| `CRSH` | `aa-b` | drive `aa`, bit crush `b` (samples); synths use `aa` as drive |

## Samples only

| Command | Value | Does |
| --- | --- | --- |
| `PLAY` | `00bb` | play mode for this note: `00` forward, `01` reverse, `02` loop, `03` rev-loop, `04` pingpong, `05` rev-pingpong, `06`-`08` osc, `09` loop-sync ([play modes](Samples#play-modes)). With a note: the note starts over in that mode (reverse from E); without: the sound turns around from where it is |
| `PLOF` | `aabb` | jump to position `aa`/256 of the sample, or move by `bb` chunks |
| `LPOF` | `aaaa` | shift the loop window (wavetable scanning, stretch) |
| `SLCE` | | pick a slice |
| `FBMX` / `FBTN` | `aabb` | feedback mix / tune |
| `IRTG` | `aabb` | retrigger and transpose |

## MIDI

| Command | Does |
| --- | --- |
| `MDCC` | send MIDI CC `aa` with value `bb` |
| `MDPG` | send program change |
| `MVEL` | set note velocity |

## Tables: commands that play themselves

<img src="images/table.png" width="300" align="right" alt="Table">

A table is a little 16-row loop of commands that advances one row per tick. Point an instrument at it (Instrument → synth LFO page or sample Motion page → `table`, `auto`) or start it from a phrase with `TABL`.

Example — a trance gate on a pad:

```text
00  VOLM 00FF
01  VOLM 0000
02  HOP  0000   (jump back to row 00)
```

Example — a fast octave, root, fifth arpeggio:

```text
00  PTCH 000C
01  PTCH 0000
02  PTCH 0007
03  HOP  0000
```
