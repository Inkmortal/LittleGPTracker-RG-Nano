# Controls

The RG Nano has few buttons, so almost everything is **a button held + a D-pad direction**. Three rules cover most of it:

| Hold | + D-pad does |
| --- | --- |
| **A** | change the value under the cursor (Left/Right small step, Up/Down big step) |
| **RB** | jump to another screen |
| **B** | jump to the neighbouring chain / phrase / instrument (on Song: 16 rows up/down) |

**Start** plays and stops. **RB + Select** opens the helper on every screen.

> On the RG Nano, **Select** is the `FN` button. **LB/RB** are the shoulder buttons. The Menu/Power button opens the app menu: **Volume** and **Brightness** (Left/Right), **Save and quit**, **Quit, don't save** (asks first) and **Debug tools**. Holding power to switch off saves your song first.

## Moving between screens

<img src="images/help-1-map.png" width="300" align="right" alt="Helper map page">

Hold **RB** and press a direction:

| From | RB + Right | RB + Left | RB + Up | RB + Down |
| --- | --- | --- | --- | --- |
| Song | Chain under the cursor | — | Project | Mixer |
| Chain | Phrase under the cursor | Song | — | — |
| Phrase | Instrument of the note | Chain | Groove | Table |
| Instrument | — | Phrase | list of all sounds | Instrument table |
| Table | Instrument table | Table (from the instrument table) | back up | — |
| Groove | — | — | — | Phrase |
| Project | — | — | — | Song |
| Mixer | — | — | Song | FX |
| FX | — | — | Mixer | — |

If a cell is empty, RB + Right tells you to press **A** first. Holding RB + a direction moves one screen, however long you hold it.

<br clear="right">

## Everywhere

| Input | Does |
| --- | --- |
| **Start** | play / stop (what plays depends on the screen, see below) |
| **RB + Start** | play / stop the whole song from a Chain, Phrase, Table, Groove or Instrument screen |
| **RB + Select** | helper: **Down/Up** flips map → commands → how-to, **RB + Select** closes |
| **A + RB** | solo the track (Song, Chain, Phrase) |
| **B + RB** | mute the track (Song, Chain, Phrase); on the Mixer it's **LB + A** / **RB + A** |
| **B + LB** | start a selection; **B** copies it, **A + LB** pastes |

Each combo does one thing on a screen: nothing is mapped twice, and a combo never fires two actions at once.

The small label at the top right says what is playing: `PLAY:SONG`, `PLAY:CHAIN`, `PLAY:PHR`, `PLAY:LIVE`, `AUDITION` or `STOP`.

## Song

| Input | Does |
| --- | --- |
| D-pad | move between rows (time) and the 8 tracks |
| **A** on `--` | put the last-used chain here (screen says `Reused 00`) |
| **A** again | replace it with a brand-new empty chain (`New chain 01`) |
| **A + D-pad** | change the chain number |
| **B + A** | delete the chain from this cell |
| **B + Up/Down** | 16 rows up / down |
| **LB + Up/Down** | jump through song sections |
| **LB + Left/Right** | nudge the tempo |
| **Start** | play the song from this row |
| **B + Start** | while playing, jump every track to this row right away |
| **Select** | switch to **Live mode** and back (see below) |

## Live mode

Jam with your song: loop sections, bring parts in and out, switch sections on the beat. **Select** on the Song screen switches Song ↔ Live; the title says which, and the buttons are listed at the bottom of the screen.

| Input (Live) | Does |
| --- | --- |
| **Start** on a cell | cue that chain on its track: it starts when the track's current chain ends, then loops. Press twice to switch at the next bar instead |
| **LB + Start** | cue the whole row: every track switches to this section together |
| **RB + Start** | cue a stop for this track (twice: stop at the next bar) |
| **B + Start** | stop everything now |
| **B + RB** / **A + RB** | mute / solo a track while it plays |

A cued cell blinks in green until it starts. A track keeps looping its chain until you cue something else, so you can leave the drums on the verse while the bass moves to the chorus. **LB + Start** also switches to Live by itself. The Mixer, and Start on any other screen, work as usual.

## Chain

| Input | Does |
| --- | --- |
| **A** on `--` | put the last phrase here; **A** again for a new empty phrase |
| **A + D-pad** | change phrase number, or transpose (second column) |
| **B + D-pad** | jump to the next/previous chain |
| **Start** | play this track's chain on a loop |
| **LB + Start** | record the chain into a new sample ([render to sample](Samples#render-to-sample)) |

## Phrase

| Input | Does |
| --- | --- |
| **A** on an empty step | add a note (copies the last note and instrument) |
| **A + Left/Right** | note down/up a semitone (or a scale step if a key is set) |
| **A + Up/Down** | note down/up an octave |
| **LB + D-pad** on a note | chromatic edit that ignores the key/scale |
| **A + D-pad** on the `I` column | pick the instrument |
| **Select** on a command column | open the command picker |
| **A + Up/Down** on a command | step through the commands A to Z |
| **A + Left/Right** on a command | step through the commands in list order |
| **B + A** | delete |
| **B + D-pad** | jump to the neighbouring phrase |
| **Start** | loop this bar |
| **LB + Start** | record this bar into a new sample ([render to sample](Samples#render-to-sample)) |

## Instrument

| Input | Does |
| --- | --- |
| **LB + Left/Right** | change page (SOUND, ENV, FILTER, LFO, MOD, MIX for synths) |
| **A + Left/Right** on `preset` | browse ready-made sounds |
| **A + D-pad** | change the focused knob |
| **B + Left/Right** | previous / next instrument (**B + Up/Down**: 16 at a time) |
| **B + A** | on `sample`: remove the sample; on `table`: clear it |
| **RB + Up** | the list of all sounds |
| **Start** | play / stop the phrase, same as the Phrase screen |
| **RB + A + Up** | hear the instrument at C3 |
| **RB + A + Left/Up/Right** | hear it low / middle / high |
| **RB + A + Down** | stop the preview |

Sample instruments have their own pages and trim controls — see [Samples](Samples).

## Mixer and FX

| Input | Does |
| --- | --- |
| **Left/Right** | pick a strip (Mixer) |
| **Up/Down** | pick a knob (FX) |
| **A + Up/Down** / **A + Left/Right** | big / small step |
| **LB + A** / **RB + A** | mute / solo the track (Mixer) |
| **Start** | play / stop the song |

## Naming a new song

The letters are in alphabetical order (not QWERTY), so the next letter is always one press away.

| Input | Does |
| --- | --- |
| D-pad | move over the keys and the `abc RANDOM CANCEL DONE` row (Down from the buttons wraps to the top letters) |
| **A** on `abc` / `ABC`, or **Select** | switch the keys to lowercase / uppercase |
| **A** | type the letter / run the button |
| **B** | erase a letter; on an empty name, leave without making a song |
| **LB / RB** | move the cursor inside the name |
| **Start** or `DONE` | create the song |

The suggested random name is highlighted: the first letter you type replaces it. `name taken` means a song with that name already exists.
