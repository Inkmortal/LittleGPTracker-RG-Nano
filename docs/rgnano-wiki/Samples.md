# Samples

Any instrument slot can play a WAV file instead of a synth. The RG Nano has no microphone, so the practical route is to make or record sounds elsewhere (a phone, a PO-33, a computer) and copy them over.

## Get WAVs onto the Nano

Copy `.wav` files (16-bit, mono or stereo, any sample rate) into `Applications/Samples` on the SD card. Folders are fine.

## Sample packs

The install puts a set of packs in `Applications/Samples`, one folder each. Melodic samples are recorded or rendered at a known pitch and the chords are rooted on `A 2`, so they play in tune straight away.

| Pack | What's in it |
| --- | --- |
| `keys` | grand piano (low, mid, high, soft), harp, glockenspiel, marimba |
| `strings` | violins (low, high) and cellos (low, mid), sustained; violin and cello pizzicato |
| `brass` | horn, trumpet, trombone |
| `bass` | upright (low, high), finger bass, Reese |
| `epiano` | a tine electric piano note plus maj, min, 7th and 9th chords |
| `chords` | one-shot piano, soft piano and string chords: maj, min, maj7, min7, dom7, maj9, min9 |
| `guitar` | clean, muted and power-chord guitar |
| `percussion` | timpani, orchestral snares, crash, cymbal, shaker, bongos, congas, triangle, claves |
| `drums-808` / `drums-909` | the classic drum machines: kick, snare, clap, hats, and more |
| `drums-dusty` | a lo-fi kit: soft kick, old snare, hat, rim |
| `drums-jazz` | brush tap and swish, ride and ride bell, soft kick |
| `textures` | vinyl crackle, tape hiss, a riser |
| `chinese` | guzheng, erhu, dizi, pipa, dagu and opera percussion |

The orchestral recordings come from the Versilian Studios Community Edition (CC0). The Chinese instruments are CC0 and CC BY 4.0 recordings from Freesound. The drums, guitar, bass, e-piano, chords and textures are synthesized for this project. Each folder's `CREDITS.md` has the details. The [genre demo songs](Demo-Songs#a-song-for-every-genre) use every pack.

## Turn a slot into a sampler

1. Open an instrument (**RB + Right** from a phrase).
2. On the first page, set `type` to **sample** (cursor on `type`, **A + Left**).
3. Move to `sample` and press **Select**: the sample browser opens.
4. Highlight a file. `Listen` previews it, `Import` copies it into the song and assigns it. **Start + Up/Down** browses while previewing, **Start + Right** imports.

Imported samples live inside the song folder (`lgpt_<name>/samples/`), so the song keeps working even if you tidy up `Applications/Samples`.

## Sample pages

**LB + Left/Right** switches between the seven sample pages:

| Page | What it edits |
| --- | --- |
| Source | sample, root note, detune, slices |
| Shape | volume, pan, reverb, delay and chorus sends (the shared effects on the [FX screen](Screens#fx)), crush, drive, downsample |
| Filter | cutoff, resonance, filter type and mode |
| Loop | loop mode, start, loop start, end |
| Mod | two envelopes/LFOs on volume, cutoff, reso, pitch or pan — see [Synth → MOD](Synth#mod--envelopes-and-lfos) |
| Motion | instrument table, feedback |
| EQ | the instrument's own low / mid / high EQ, drawn as a curve — see [Instrument EQ](#instrument-eq) |

## Instrument EQ

The last page. Each sample instrument has its own three-band EQ, the same as a synth's ([Synth → EQ](Synth#eq--its-own-tone)): `l.gain`/`l.freq` a bass shelf (30–400 Hz), `m.gain`/`m.freq` a bell (150 Hz–6 kHz), `h.gain`/`h.freq` a treble shelf (1.5–16 kHz). Gains `80` = flat, −12 … +12 dB. It shapes only this sound, before its effect sends; flat bands cost nothing. The curve above the knobs shows the result, and the focused knob's value is shown in dB or Hz.

**Try:** thin a loop with `l.gain` `−8 dB` so the kick has the bottom to itself, or brighten a dull vocal chop with `h.gain` `+4 dB`.

## Trimming

Source and Loop show the waveform with three markers: **S** start, **L** loop start, **E** end.

| Input | Does |
| --- | --- |
| **LB + Up/Down** | pick the marker |
| **LB + A + Left/Right** | nudge it |
| **A + Start** | hear the sample from S to E (**RB + A + Left/Right**: an octave down/up) |
| **RB + Start** | switch that preview between once and loop (`PREV:` on screen) |

## Root note

If a melodic sample plays out of tune, go to `root` and press **Select**: the app listens to the trimmed part and suggests a root note. **Select** again accepts it.

## Render to sample

Resampling, M8 style: on a Phrase press **LB + Start** to record that bar, or on a Chain to record the whole chain. It plays once, with every track, effect and fader as you hear it, and lands as `rs_01.wav` on the next free instrument. **A** opens the new instrument; **B** cancels and leaves nothing behind. Chop it with `slices`, reverse it with a table, or stack it again.

## Slices

Set `slices` above `01` to chop the sample into equal parts; note `C-2` plays slice 0, the next semitone slice 1, and so on — handy for drum loops.
