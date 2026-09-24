# Samples

Any instrument slot can play a WAV file instead of a synth. The RG Nano has no microphone, so the practical route is to make or record sounds elsewhere (a phone, a PO-33, a computer) and copy them over.

## Get WAVs onto the Nano

Copy `.wav` files (16-bit, mono or stereo, any sample rate) into `Applications/Samples` on the SD card. Folders are fine.

## Turn a slot into a sampler

1. Open an instrument (**RB + Right** from a phrase).
2. On the first page, set `type` to **sample** (cursor on `type`, **A + Left**).
3. Move to `sample` and press **Select**: the sample browser opens.
4. Highlight a file. `Listen` previews it, `Import` copies it into the song and assigns it. **Start + Up/Down** browses while previewing, **Start + Right** imports.

Imported samples live inside the song folder (`lgpt_<name>/samples/`), so the song keeps working even if you tidy up `Applications/Samples`.

## Sample pages

**LB + Left/Right** switches between the six sample pages:

| Page | What it edits |
| --- | --- |
| Source | sample, root note, detune, slices |
| Shape | volume, pan, reverb and delay sends (the shared FX set on the Project screen), crush, drive, downsample |
| Filter | cutoff, resonance, filter type and mode |
| Loop | loop mode, start, loop start, end |
| Mod | two envelopes/LFOs on volume, cutoff, reso, pitch or pan — see [Synth → MOD](Synth#mod--envelopes-and-lfos) |
| Motion | instrument table, feedback |

## Trimming

Source and Loop show the waveform with three markers: **S** start, **L** loop start, **E** end.

| Input | Does |
| --- | --- |
| **LB + Up/Down** | pick the marker |
| **LB + A + Left/Right** | nudge it |
| **A + Up/Down** on the `sample` row | pick the marker |
| **A + Left/Right** on the `sample` row | nudge it (add **RB** for big steps) |
| **Start** | preview from S to E |
| **RB + Start** | toggle loop preview |

## Root note

If a melodic sample plays out of tune, go to `root` and press **Select**: the app listens to the trimmed part and suggests a root note. **Select** again accepts it.

## Slices

Set `slices` above `01` to chop the sample into equal parts; note `C-2` plays slice 0, the next semitone slice 1, and so on — handy for drum loops.
