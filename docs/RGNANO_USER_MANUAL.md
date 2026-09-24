# RG Nano User Manual

This manual is for the RG Nano fork of LittleGPTracker. It explains the practical production workflow on the 240x240 RG Nano screen.

The short version: Song chooses chains, chains choose phrases, phrases contain notes and commands, instruments choose the sounds, and tables/grooves add motion and feel.

New to trackers? Start with [TRACKER_BASICS.md](TRACKER_BASICS.md): a from-zero walkthrough (first beat, bassline, chords, song structure, music theory cheat sheet).

## Device Basics

| RG Nano button | Main use |
| --- | --- |
| D-pad | Move around the current screen. |
| A | Confirm, paste/create, or edit with D-pad. |
| B | Back, cut/copy/select actions, or alternate edits with D-pad. |
| L | Left shoulder modifier for jumps, chromatic note override, and some clipboard actions. |
| R | Right shoulder modifier for screen navigation and song-context actions. |
| Start | Play or stop the current context. |
| Select / FN | Contextual helper or command picker, depending on where you are. |
| Menu / Power | LGPT app exit/debug menu on single press. |

The simulator keyboard mapping is listed in [RGNANO_SIM.md](RGNANO_SIM.md).

## The Tracker Stack

| Layer | What it means | FL Studio-ish comparison |
| --- | --- | --- |
| Song | The full timeline. Rows go downward through time, columns are the 8 channels. | Playlist. |
| Chain | A short ordered list of phrases for one Song cell/channel. | A pattern playlist for one lane. |
| Phrase | The actual notes, instruments, and commands. | Piano roll/pattern data. |
| Instrument | The sample or sound settings used by phrase notes. | Channel/instrument settings. |
| Table | A small command sequence for repeated movement. | Automation/macro pattern. |
| Groove | Timing values that change the feel/swing. | Groove/swing template. |
| Mixer | Playback monitor for channel activity and waveform feedback. | Mixer/level monitor, not full mastering yet. |

When multiple chains are placed on the same Song row, they play at the same time, one chain per channel. A song is not only 8 chains long; each Song row can hold up to 8 simultaneous chain references, and later rows continue the arrangement.

## Main Navigation

Hold `R` and tap a D-pad direction to move between major screens:

| From | Combo | Goes to |
| --- | --- | --- |
| Song | `R + Right` | Chain for the selected Song cell, if that cell contains a chain. |
| Song | `R + Down` | Mixer. |
| Song | `R + Up` | Project/settings. |
| Chain | `R + Left` | Song. |
| Chain | `R + Right` | Phrase for the selected Chain row, if it contains a phrase. |
| Phrase | `R + Left` | Chain. |
| Phrase | `R + Right` | Instrument for the selected/nearest instrument. |
| Phrase | `R + Down` | Phrase Table. |
| Phrase | `R + Up` | Groove. |
| Instrument | `R + Left` | Phrase. |
| Instrument | `R + Down` | Instrument Table. |
| Table | `R + Up` | Phrase or Instrument, depending on table type. |

## Universal Helper

`R + Select` opens the universal helper from every visible LGPT screen: tracker views, modal dialogs, and the Project selector you see before a song is loaded. On the RG Nano hardware, `Select` means the physical `FN`/Select button; the Menu/Power button opens the app menu.

Page 1 is the minimap. It shows:

- the current screen name
- the current screen highlighted inside the screen map
- the main Song > Chain > Phrase relationship
- where to navigate next
- the main edit gesture for the current screen

Press `Up` or `Down` while the helper is open to switch to page 2.

Page 2 is the command page. It shows:

- the most useful buttons for the current screen
- short descriptions for each input
- playback/context hints

On the Instrument screen, the helper follows the current sample lab page. Source, Shape, Filter, Loop, and Motion each show their own compact controls instead of one generic instrument reminder. Source and Loop explicitly list the trim controls. On the Source sample row, `Select` opens sample import, `A + Up/Down` chooses `START`, `LOOP`, or `END`, and `A + Left/Right` nudges the active marker without changing the assigned sample. Add `RB` to that nudge for a faster move. The shoulder combo still works too: `LB + Up/Down` chooses the marker and `LB + A + Left/Right` nudges it. The normal field list below the waveform remains available for exact start, loop-start, and end values.

Press `RB + Select` again to close the helper. While the helper is open, normal inputs are blocked so you can read without accidentally changing music, confirming a dialog, or deleting a project.

## Playback Scope

The RG Nano fork shows a small playback label on main screens:

| Label | Meaning |
| --- | --- |
| `STOP` | Nothing is playing. |
| `PLAY:SONG` | Song/arrangement context is playing. All active channels can run. |
| `PLAY:CHAIN` | Current channel chain context is playing. |
| `PLAY:PHR` | Current channel phrase context is playing. |
| `PLAY:LIVE` | Live/queued chain mode is active. |
| `AUDITION` | A note/sample preview is playing. |

Important playback controls:

| Screen | Input | Behavior |
| --- | --- | --- |
| Song | `Start` | Play/stop song context from the selected row. |
| Song | `L + Start` | Start current row in live mode. |
| Song | `R + Start` | Stop or song-context transport action. |
| Chain | `Start` | Play/stop the current channel chain. |
| Chain | `R + Start` | Play/stop in song context. |
| Phrase | `Start` | Play/stop the current channel phrase. |
| Phrase | `R + Start` | Play/stop in song context so you can hear the phrase with the arrangement around it. |
| Table/Instrument/Groove | `Start` | Play/stop phrase/current-channel context. |

This is the answer to the common producer problem: if you are writing melody and want drums/bass around it, use song context. If you want to focus tightly on only the thing you are editing, use the normal Start behavior on Chain/Phrase.

## Song View

Song View is the top timeline.

| Thing on screen | Meaning |
| --- | --- |
| Row numbers | Time positions in the arrangement. |
| 8 columns | The 8 tracker channels/tracks. |
| Cell value like `00` | Play chain 00 on this channel at this row. |
| `--` | Empty cell; nothing starts there for that channel. |
| Bottom activity strip | Live note/instrument/meter feedback while playback runs. |

Basic actions:

| Input | Behavior |
| --- | --- |
| D-pad | Move between rows/channels. |
| `A` | Paste/create a chain in an empty cell. |
| `A + D-pad` | Change chain value. |
| `L + Left/Right` | Nudge tempo. |
| `L + Up/Down` | Jump song sections. |
| `R + Right` | Open selected Chain. |
| `R + Down` | Open Mixer. |

## Chain View

Chain View is a playlist of phrases for one Song cell/channel.

| Field | Meaning |
| --- | --- |
| Phrase | Which phrase number plays at this chain row. |
| Transpose | Pitch offset for that phrase. |

Basic actions:

| Input | Behavior |
| --- | --- |
| D-pad | Move row/field. |
| `A` | Paste/create phrase on phrase column. |
| `A + D-pad` | Edit phrase or transpose value. |
| `R + Left` | Return to Song. |
| `R + Right` | Open selected Phrase. |

## Phrase View

Phrase View is where you write notes and tracker commands.

| Field | Meaning |
| --- | --- |
| Note | Pitch or rest. |
| Instrument | Which instrument/sample slot plays the note. |
| Command | Tracker effect command. |
| Value | Four-digit command parameter. |
| Mini waveform/activity strip | Playback feedback while you work. |

Basic actions:

| Input | Behavior |
| --- | --- |
| D-pad | Move between rows/fields. |
| `A` | Preview/paste depending on field. |
| `A + D-pad` | Edit note/instrument/command/value. |
| `L + D-pad` on note | Quick chromatic note edit, ignoring project key/scale. |
| `Select` on command columns | Open command selector/help picker. |
| `R + Left` | Return to Chain. |
| `R + Right` | Open Instrument. |
| `R + Down` | Open Phrase Table. |
| `R + Up` | Open Groove. |

New projects default to Project `Key: --`, which means normal chromatic editing. If you set a key/scale later, note edits can follow that scale, and `L + D-pad` remains the quick outside-note override.

## Synth Instruments

Every new project starts with 16 synth instruments, so notes make sound immediately without any samples:

`00 KICK  01 SNARE  02 HAT  03 OPENHAT  04 CLAP  05 BASS  06 LEAD  07 PAD  08 PLUCK  09 KEYS  0A BELL  0B ACID  0C SUBBASS  0D CHIP  0E TOM  0F PERC`

Open one with `R + Right` from Phrase. The synth screen has six pages (`LB + Left/Right`):

| Page | Knobs |
| --- | --- |
| SOUND | `type` (synth/sample), `preset`, `wave` (sine, triangle, saw, pulse, supersaw, noise, metal), `shape`, `sub`, `noise`, `fm`, `ratio`, `chord`, `tune` |
| ENV | `attack`, `decay`, `sustn`, `releas`, `p.env` (pitch drop), `p.dec`, `glide` |
| FILTER | `filter` (lowpass/highpass/bandpass/off), `cutoff`, `reso`, `env` (brightness burst), `envdec`, `drive` |
| LFO | `lfo` target (pitch/cutoff/volume/shape), `rate`, `depth`, `fine`, instrument table `auto`/`table` |
| MOD | two slots: envelope (`decay`/`swell`) or LFO (`sine`/`triangle`/`square`/`saw`/`random`) aimed at volume, cutoff, reso, pitch or pan, with `amt` and `rate` |
| MIX | `volume`, `pan`, `reverb` send, `delay` send |

The top of each page draws what the knobs do (waveform, envelope, filter curve, LFO, levels), and the lines under it explain the focused knob with real units. When you open an instrument the cursor lands on `preset`: `A + Left/Right` there auditions every ready-made sound. `Start` plays the sound at C3, `RB + A + Left/Up/Right` plays low/mid/high, `RB + A + Down` stops.

Synths understand the usual commands (`VOLM`, `PAN`, `FCUT`, `FRES`, `FLTR`, `PTCH`, `LEGA`, `PFIN`, `ARPG`, `RTRG`, `KILL`, `TABL`) plus `CHRD`: `CHRD abcd` adds notes `a`, `b`, `c`, `d` semitones above the played note, all sounding together (`0047` major, `0037` minor, `047B` major 7, `037A` minor 7). `RTRG` restrikes the envelope for rolls and tremolo.

Notes stopped by `KILL` or by a different instrument on the same track fade out with the instrument's release instead of cutting off.

Reverb and delay are shared by every instrument. Set their character on the Project screen: `Reverb` (room size), `Damp` (darker tail), `Echo` (delay time in 16th notes, 3 = dotted 8th), `Fdbk` (number of repeats). Each instrument chooses how much it sends on its MIX page.

To use a WAV instead, set `type` to `sample` on the first page; the sample lab pages below take over.

## Instruments And Samples

The RG Nano has no built-in microphone. The practical workflow is uploaded samples:

1. Put `.wav` files in the configured sample folder.
2. Open Instrument View from Phrase with `R + Right`.
3. Move to the sample field.
4. Press `Select` to open sample import.
5. Use `Listen` to preview.
6. Use `Import` to copy the sample into the project.

When a sample imports, the app may create a quiet root-note suggestion, but it does not change the instrument `root` automatically. Obvious drum/noise filenames such as kick, snare, hat, drum, perc, vinyl, and FX stay manual. Percussion detection uses filename tokens, so a melodic file named something like `trimmed_erhu.wav` will not be mistaken for a rim shot just because it contains the letters `rim`.

Long samples are different: the first obvious pitch may not be the part you actually want to play. Trim the sample first with the Source or Loop page markers, move focus to the `root` field, then press `Select`. If no suggestion is visible, `Select` analyzes only the current `START` to `END` window and shows `suggest root ###`. Press `Select` again to accept that suggestion, or ignore it and edit the root normally by ear.

The Instrument screen now splits sample design into five compact lab pages. Use `LB + Left/Right` to switch pages:

| Page | What it edits |
| --- | --- |
| Source | Sample assignment, root note, detune, slices. |
| Shape | Volume, pan, crush, drive, downsample, interpolation. |
| Filter | Cutoff, resonance, filter type, mode, attenuation. |
| Loop | Loop mode, slices, sample start, loop start, loop end. |
| Mod | Two envelopes/LFOs, same as the synth MOD page. |
| Motion | Instrument table automation and feedback movement. |

On the Source and Loop pages, the waveform has three editable markers: `START`, `LSTART`, and `END`. The screen now spells out the active edit target as `EDIT START`, `EDIT LSTART`, or `EDIT END`, and shows the three marker positions as `S`, `L`, and `E` values below the waveform. On Source, keep focus on the `sample` row and use `A + Up/Down` to choose the active marker, then `A + Left/Right` to nudge it. Hold `RB` as well for a faster nudge. These shortcuts never change the assigned sample; `Select` is the import action. The original shoulder controls also remain: `LB + Up/Down` chooses the active marker and `LB + A + Left/Right` nudges it. The app keeps the trim order valid, so `START` cannot move past `LSTART`/`END`, and `END` cannot move left of the current loop/start range. The normal field list remains available below the visual panel for exact parameter editing. The waveform itself stays magenta; bright white is reserved for the active marker so the trim selection is easier to read on the tiny screen.

To check whether a sample still feels musical after pitch shifts, hold `RB + A` and tap `Left`, `Up`, or `Right` for low, root, or high audition notes. `RB + A + Down` stops that audition. On the Source `sample` row, `RB + A + Left/Right` is reserved for fast trim movement instead. This keeps sample design on the Instrument screen instead of forcing a trip back to Phrase every time you tweak root note, loop points, filter, or volume.

Press `Start` on a sample Instrument page to preview the current sample at its root note. The default preview is `PREV:ONCE`: it plays from `START` to `END` and stops, regardless of the instrument's saved loop mode. Press `RB + Start` to toggle `PREV:LOOP`; in loop preview, the attack plays from `START`, then playback sustains by repeating `LSTART` to `END`. This preview toggle is for listening while trimming and does not rewrite the instrument's saved loop mode.

This fork aims for M8-like pocket production, but native M8-style synth engines and deep offline sample editing are still future work.

## Tables And Groove

Tables are tiny command patterns. Use them for movement:

- arps
- retrigs
- pitch movement
- filter movement
- repeating command effects

Groove changes timing feel and swing. Small groove edits can make loops feel much less rigid.

## Rendering WAVs

Use Project View when you want to bounce the tracker song into audio files:

1. From Song View, press `R + Up` to open Project.
2. Set `Render:` to `Stereo` for one full mix, or `Stems` for separate channel files.
3. Press `Start` to begin song playback/rendering.
4. Press `Start` again to stop and close the WAV file.

`Stereo` writes `mixdown.wav` into the active project folder. `Stems` writes per-channel files such as `channel0.wav`, `channel1.wav`, and so on. The simulator now verifies both modes by checking the native files the app writes, not an external capture.

## Current Limits

Implemented and useful today:

- 8-channel tracker arrangement
- sample import/listen workflow
- Song/Chain/Phrase editing
- Instrument, Table, Groove, Mixer, Project screens
- universal helper
- visible playback scope
- native stereo/stems WAV rendering from Project View
- simulator workflows for navigation, commands, playback scope, render/export, and audio capture

Still incomplete compared with Dirtywave M8:

- native synth engines
- deep sample editor: crop, normalize, slice, resample
- dedicated M8-style render view and render-to-sample workflow
- full EQ/limiter/send/mixer parity
- hardware live sampling path

The current design philosophy is: M8-style power in a much smaller pocket device, with compact screens and strong shortcuts instead of trying to show everything at once.
