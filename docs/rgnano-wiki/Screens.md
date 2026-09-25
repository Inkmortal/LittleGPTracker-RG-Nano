# Screens

Every screen at a glance. For the full button list see [Controls](Controls).

## Song

<img src="images/demo-song-playing.png" width="300" align="right" alt="Song">

The arrangement. Columns are the 8 tracks, rows are time. Each cell holds a chain number, or `--` for nothing.

- Everything on one row starts together.
- Rows play top to bottom; each track loops back to the top of its block when it reaches `--`.
- While playing, the strip at the bottom shows each track's current note and instrument.
- The eight thin bars to the right of the grid are level meters, one per track: the track you're on is highlighted, muted tracks go dim, and a bar gets an amber cap when it's close to full.

<br clear="right">

### Live mode

<img src="images/song-live.png" width="300" align="right" alt="Live mode">

**Select** turns the Song screen into a clip launcher. **Start** on a cell cues that chain on its track (it blinks green until it starts, then loops), **LB + Start** cues the whole row, **RB + Start** cues a track to stop, **B + Start** stops everything. The buttons are listed at the bottom of the screen. Full table: [Controls → Live mode](Controls#live-mode).

<br clear="right">

## Chain

<img src="images/demo-chain.png" width="300" align="right" alt="Chain">

One track's playlist of bars. Left column: phrase number. Right column: transpose in semitones (`0C` = up an octave, `F4` = down an octave).

Next to each row is a tiny piano roll of that row's phrase, so you can see the melody or rhythm of every bar without opening it. The row that's playing lights up, with a playhead moving across it.

<br clear="right">

## Phrase

<img src="images/demo-phrase-melody.png" width="300" align="right" alt="Phrase">

One bar, 16 steps. Columns:

1. **Note** — `C 3`, `----` for none
2. **Instrument** — `I00`…`I7F`; the name of the instrument under the cursor shows in the title
3. **Command 1** + value
4. **Command 2** + value

Step numbers are one digit, `0`–`F`. Beat rows (`0 4 8 C`) are highlighted so you can see the grid, and the playing step lights up as a green tab.

<img src="images/demo-phrase-playing.png" width="300" align="right" alt="Phrase playing">

Under the grid is a piano roll of the whole bar: each note sits at its pitch, with a thin line until the next note or `KILL`. A playhead follows playback and an amber mark shows the step your cursor is on. The line at the very bottom is a live waveform of the output.

<br clear="right">

## Instrument (synth)

<p>
<img src="images/synth-1-sound.png" width="190" alt="SOUND">
<img src="images/synth-2-env.png" width="190" alt="ENV">
<img src="images/synth-3-filter.png" width="190" alt="FILTER">
<img src="images/synth-5-mod.png" width="190" alt="MOD">
<img src="images/synth-6-mix.png" width="190" alt="MIX">
</p>

Six pages (SOUND, ENV, FILTER, LFO, MOD, MIX), switched with **LB + Left/Right**. The top half draws what the page does — waveform, envelope, filter curve, LFO, modulation, levels — and shows the preset name and the focused value in real units (`32 ms`, `641 Hz`). The knobs are listed below. Full details on the **[Synth](Synth)** page.

The first row, `type`, switches the slot between **synth** and **sample**.

## Instrument list

<img src="images/demo-instrument-list.png" width="300" align="right" alt="Instrument list">

**RB + Up** on the Instrument screen opens every instrument at once: number, type (`SMP`, `SYN`, `MID`, `---` for an empty slot), name, and how many phrases use it. A green dot marks instruments that are playing right now, and the box underneath draws the selected sound: a sample's waveform, or a synth's wave shape.

| Button | Does |
| --- | --- |
| Up / Down | pick an instrument (**Left/Right** jump a page) |
| A | open it on the Instrument screen |
| Start | hear it; Start again stops |
| Select | give it a name with the on-screen keyboard (clear the name to go back to the automatic one) |
| LB + A | copy it into the next free slot, to make a variation |
| B | back |

<br clear="right">

## Instrument (sample)

Sample instruments have six pages too: Source, Shape, Filter, Loop, Mod, Motion, with a waveform and start/loop/end markers. See **[Samples](Samples)**.

## Table

<img src="images/table.png" width="300" align="right" alt="Table">

Three command columns that step one row per tick and loop. Tables run on their own once triggered — by `TABL` in a phrase, or automatically by an instrument (`table` on the synth LFO page or the sample Motion page). Use them for arpeggios, stutters, pitch wobbles and filter wiggles you don't want to write note by note.

<br clear="right">

## Groove

<img src="images/groove.png" width="300" align="right" alt="Groove">

How many ticks each step lasts, cycling. `06 06` is straight. `07 05` swings (long-short). `08 04` shuffles hard. Groove `00` is used by default; the `GROV` command switches a track to another groove.

The ruler on the right draws one bar twice: an even grid on top, and this groove underneath with each step as wide as its ticks. Late offbeats are the swing. Under it the feel is named in plain words (`straight`, `swing 58%`), and the step that's playing turns green.

<br clear="right">

## Project

<img src="images/project.png" width="300" align="right" alt="Project">

| Field | What |
| --- | --- |
| Tempo | BPM |
| Master / Drive | output level / level into the soft clipper |
| Clip | soft clipper strength (Bypass … Insane) |
| Transpose | shifts every note in the song |
| Key / Scale / Notes | note-editing helper and sharp/flat spelling |
| Song List / Save Song / Save Song As | back to the song list (offers to save first), save, save a copy |
| Remove unused chains / sounds | tidy up chains, phrases and instruments nothing uses |
| Quit | leave the app (offers to save first) |
| Render | `Stereo` or `Stems`, then **Start** to export ([Export](Export)) |

<br clear="right">

## Mixer

<img src="images/demo-mixer.png" width="300" align="right" alt="Mixer">

Faders for the 8 tracks, the three effect returns (**C** chorus, **D** echo, **R** reverb) and the master (**M**), each with a live level meter, plus the master waveform. Levels save with the song.

| Input | Does |
| --- | --- |
| **Left/Right** | pick a strip |
| **A + Up/Down** | fader big step (`C0` = unity, up to `FF` for a boost) |
| **A + Left/Right** | fader fine step |
| **B + RB** / **A + RB** | mute / solo the track (**RB + LB** unmutes all) |
| **RB + Down** | FX screen |

<br clear="right">

## FX

<img src="images/fx.png" width="300" align="right" alt="FX">

The three effects every instrument can send to, each with a picture of what it does:

| Effect | Knobs | Picture |
| --- | --- | --- |
| Chorus | `speed` (0.1–5 Hz), `depth` (0.5–7.5 ms) | the left and right delay sweeps |
| Echo | `time` in 16ths (`3` = dotted 8th), `fdbk` repeats | the dry hit and each echo over two bars |
| Reverb | `size`, `damp` | the tail over four seconds; the bright line is the highs, which `damp` takes away faster |

Each heading shows the real values (Hz, ms, tail length) and how many sounds send to it. **Up/Down** walks the six knobs, **A + D-pad** edits. Turn up an instrument's `chorus`/`delay`/`reverb` send to hear it; the Mixer's C/D/R faders set how loud each effect comes back.

<br clear="right">

## Helper (RB + Select)

<p>
<img src="images/help-1-map.png" width="240" alt="Map page">
<img src="images/help-2-commands.png" width="240" alt="Commands page">
<img src="images/help-3-howto.png" width="240" alt="How-to page">
</p>

Available on every screen, including dialogs. **Down/Up** flips between:

1. **Map** — where you are and where RB + direction goes, plus the undo keys
2. **Commands** — the buttons for this screen, then **MORE KEYS** with the rest. On the synth pages it also explains the knob under the cursor.
3. **How to** — a short walkthrough for this screen

On any page, **A** opens the built-in guide at the section for the current screen.

While the helper is open, other buttons are ignored, so you can't change anything by accident.
