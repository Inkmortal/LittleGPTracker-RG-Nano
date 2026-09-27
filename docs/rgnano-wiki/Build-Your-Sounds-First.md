# Build Your Sounds First

Some people write notes first and find sounds later. If you'd rather **assemble your instruments first, then compose**, the **Rack** is for you: every sound of the song on one screen, where you play them, browse new ones by ear, shape them, and save your favourites for the next song.

## The Rack

From the **Song** screen, **RB + Left** opens the Rack.

```
Rack your sounds      05/8F
   kind name
00 SYN KICK           2
01 SYN SNARE          2
05 SYN BASS
06 SMP rgnano-tone
in 2 phrases
[ picture of the sound ]
note C3 steps in the scale
[ one octave keyboard ]
```

Each row is a slot: its number, its kind (`SYN` synth, `MAC` macro synth, `SMP` sample, `---` empty) and name, and how many phrases use it. The box draws the selected sound, and the keyboard shows the song's scale (set it on the [Scale](Screens#scale) screen) with a bar under the note you'll hear.

| Keys | Does |
| --- | --- |
| **Up / Down** | pick a sound (**Left / Right**: a page) |
| **A** (hold) | play it; let go of **A** and it stops |
| **A + Left / Right** | the next note of the scale down / up, and play it (no key set: a semitone) |
| **A + Up / Down** | an octave up / down, and play it |
| **Select** | the [sound browser](#the-sound-browser): hear sounds as you move, take one |
| **RB + Right** | edit it on the Instrument screen; **RB + Left** there comes back to the Rack |
| **LB + A** | copy it to a free slot, to make a variation |
| **B** | back to the Song |

The keyboard starts where the sound sounds natural: a synth at C3, a sample at the note it was recorded at (its root).

> Picking a sound on the Rack also picks it for your next note: the next note you add on a Phrase screen uses it.

## The sound browser

**Select** on the Rack opens every sound you can put in the slot, by kind:

| Kind | What's in it |
| --- | --- |
| My sounds | the sounds you saved (see [Save and load](#save-and-load)) |
| Synth presets | the synth's ready-made sounds, one list per engine (FM4, Hyper, Wav ...) |
| Macro synth presets | the [Macro Synth](Macro-Synth)'s |
| Samples | the sample packs on the card, by folder |

**A** opens a kind (or a folder). Then just move: **every sound plays as you land on it**, at the Rack's keyboard note. A synth sound is put in the slot to be heard; a sample streams from the card. **Start** plays it again.

- **A** takes it: it stays in the slot, and you're back on the Rack.
- **B** puts the slot back exactly as it was and goes back one step (to the kinds, or up a folder).

So you can try twenty kicks and still walk away with your old one.

## Save and load

Your sounds live on the card in `Applications/Sounds`, outside any song, so they're there in every song. **LB + Start** on the instrument list (**RB + Up** on the Instrument screen) opens **My sounds**:

| Choice | Does |
| --- | --- |
| Save sound 05 | keeps the selected sound under a name; a sample sound takes a copy of its WAV, and its table comes along |
| Load a sound into 05 | puts a saved sound in the slot (the browser's **My sounds** does too, with a listen first) |
| Save this kit | keeps all of this song's sounds, each in its slot |
| Load a kit | replaces all of this song's sounds with a saved kit (it asks first) |
| New songs: this kit | every new song starts with this song's sounds instead of the starter kit |
| New songs: starter kit | back to the built-in starter kit |

The name is filled in for you (the sound's name, or the song's for a kit): **A** saves straight away.

## A kit in five minutes

A drum kit and a bass, built before a single note:

1. Song **RB + Left**: the Rack, on `00 KICK`.
2. **Select**, **A** on `Synth presets`, then **Down** through the list: each one plays. **A** on the kick you like.
3. **Down** to `01`, **Select** again: a snare this time. Try the `Samples` too.
4. **Down** to `05`: the bass. Hold **A** and step through the scale with **A + Right** to hear it across notes.
5. Too dark? **RB + Right**, open the filter on the FILTER page, **RB + Left** back.
6. **RB + Up** on the Instrument screen, **LB + Start**, `Save this kit`, **A**. Then `New songs: this kit` if every song should start with it.

Now write the notes: every sound is already where you want it.

```phrase
00 C 3 00 ----
01 --- -- ----
02 C 3 05 ----
03 --- -- ----
04 C 3 00 ----
```

The `I` column (the second one) says which slot plays each note: `00` the kick, `05` the bass you built.
