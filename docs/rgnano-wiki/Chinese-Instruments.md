# Chinese Instruments

A kit of Chinese instruments, built from the synth engines: plucked zithers and lutes, bowed and blown melody instruments, gongs, bells and temple drums. This page shows how each one is played, and how to make a tracker phrase sound like a player instead of a machine.

## What makes them sound Chinese

The big sample libraries (Kong Audio's Chinee series is the one players rate highest) sound real because of **how the notes are played**, not just the tone: pitch slides between notes, grace notes, vibrato that grows as a note is held, tremolo on plucked strings, glissandos. Get those right and even a simple tone sounds like an erhu. The presets do part of it for you:

- **Scoops**: the winds and the erhu start each note a little flat and slide up into it (mod slot 1).
- **Late vibrato**: their vibrato fades in after the note has settled, like a player's (mod slot 2, an LFO with a `fade`).
- **Slides**: `glide` is on, so a note right after another slides to it.

The rest you play with commands, below.

## Load the kit

1. On the Song screen, **RB + Left** opens the Rack.
2. **LB + Start**: My sounds. Pick `Load a kit`, then `Chinese`.
3. It asks "Replace every sound?": **Left**, then **A** (Yes).

> Loading a kit replaces every sound in the song. Load it into a new song, or save your own sounds first.

Every sound is also a preset you can pick one by one: in the Rack, **Select** opens the sound browser, and `Chinese instruments` lists them all. You hear each one as you move onto it.

| Slot | Sound | Kind |
| --- | --- | --- |
| `00` | guzheng | zither, plucked |
| `01` | guzheng bend | pressed bend after the pluck |
| `02` | guzheng run | a run up, then the note |
| `03` | pipa | lute, plucked |
| `04` | pipa lun | pipa with its tremolo |
| `05` | yangqin | hammered dulcimer |
| `06` | guqin | low zither, slides |
| `07` | erhu | two-string fiddle |
| `08` | dizi | bamboo flute |
| `09` | dizi grace | dizi with a grace note |
| `0A` | xiao | soft end-blown flute |
| `0B` | sheng | mouth organ, in 5ths |
| `0C` | suona | loud double reed |
| `0D` | bianzhong | bronze chime bells |
| `0E` | big gong | pitch sinks after the hit |
| `0F` | opera gong | pitch jumps up |
| `10` | woodblock | temple block |
| `11` | bangzi | hard clappers |
| `12` | dagu | big drum |
| `13` | tanggu | hall drum |
| `14` | bo cymbal | opera cymbals |

## The scales

Chinese music mostly uses five notes, the pentatonic. The same five notes make five modes, each starting on a different one of them. On the Scale screen they are at the end of the list:

| Scale | Notes from C | Feel |
| --- | --- | --- |
| Gong | C D E G A | bright, "folk song" |
| Shang | C D F G A# | open, floating |
| Jiao | C D# F G# A# | dark, rare |
| Zhi | C D F G A | bold, festive |
| Yu | C D# F G A# | sad, "wuxia" |

With a scale set, **A + Up/Down** on a note only steps through those five notes, so every melody fits. `SCAL` switches mode mid-song: `SCAL 002F` Gong, `0030` Shang, `0031` Jiao, `0032` Zhi, `0033` Yu (the first two digits are the key, `00` = C).

## Guzheng

The 21-string zither: bright plucks that ring, pressed bends, runs across the strings.

- **Bend** (an yin): play slot `01`, or put `PTCH 1002` on the step after the note.
- **Run** (gliss): slot `02` runs up the pentatonic into each note.
- **Tremolo**: `RTRG 0002` on a note re-plucks it very fast.

```phrase
00 A 3 00 ----
01 --- -- ----
02 C 4 00 ----
03 --- -- PTCH 1002
04 A 3 02 ----
05 --- -- ----
06 G 3 00 RTRG 0002
07 --- -- ----
```

## Pipa

The pear-shaped lute: a hard nail attack and a short ring. Its signature is the **lun**, a rolling tremolo on held notes: slot `04` does it on every note. For a single tremolo note on the plain pipa, use `RTRG 0002`; `ROLL 0042` fades one out.

```phrase
00 E 4 03 ----
01 D 4 03 ----
02 C 4 04 ----
03 --- -- ----
04 --- -- ----
05 A 3 03 ----
06 G 3 03 ----
07 A 3 04 ----
```

## Yangqin and guqin

The **yangqin** is hammered: bright metal strings in pairs, played fast. Short repeated notes and octaves suit it; `RTRG 0003` gives its drum-roll tremolo.

The **guqin** is the scholar's zither: low, dark and slow, with long notes that slide under the finger. Leave space between notes and let them ring. Its `glide` is high, so a note right after another slides; `LEGA 20FE` slides in from 2 below on purpose.

## Erhu

The two-string fiddle, the "voice" of Chinese music. Its preset scoops into notes and grows a vibrato, so the trick is in the phrasing:

- **Long notes**: give it room, the vibrato needs half a second to bloom.
- **Slides**: notes in a row slide (glide). `LEGA 10F9` slides up from 7 below: a big expressive swoop.
- **More vibrato**: `VIBR 0056` on a note for a deep, wide one.
- **Grace note**: a note one tick long just before the main note (`KILL 0001` after it).

```phrase
00 E 4 07 LEGA 10FB
01 --- -- ----
02 --- -- ----
03 D 4 07 ----
04 E 4 07 ----
05 --- -- ----
06 G 4 07 VIBR 0056
07 --- -- ----
```

## Dizi and xiao

The **dizi** is the bright bamboo flute: breathy, with the buzz of its membrane. Slot `09` flicks a grace note a step above into every note, the most common flute ornament. Quick runs suit it: notes on every step, up and down the scale.

The **xiao** is its soft, dark cousin, blown from the end: slow melodies in the low range, lots of space, `reverb` up.

## Sheng and suona

The **sheng** is a mouth organ: a bundle of bamboo pipes, each with a reed. It plays in 5ths already (`chord 5th`): hold chords of two notes or single notes, and let them breathe.

The **suona** is the loud double-reed horn of weddings and parades. It cuts through anything: short festive phrases, big scoops (`LEGA 08FC`), and a heavy vibrato.

## Percussion

Chinese opera and lion-dance drumming is loud and driving. A classic pattern:

```phrase
00 C 3 12 ----
01 --- -- ----
02 C 3 13 ----
03 C 3 13 ----
04 C 3 12 ----
05 C 3 10 ----
06 C 3 13 ----
07 C 3 14 ----
```

- **big gong** (`0E`): its pitch sinks after the hit. One on the first beat of a section.
- **opera gong** (`0F`): the "jing!" of Peking opera, its pitch jumps up.
- **bo cymbal** (`14`): the trashy clash between gong hits.
- **woodblock** / **bangzi**: the steady click that keeps opera time.
- **bianzhong** (`0D`): ancient chime bells, slow and solemn.

## A song idea

A calm-to-epic "wuxia" piece, in `Yu`:

1. **Intro**: guqin alone, slow, long notes. A bianzhong every 2 bars.
2. **Theme**: the erhu plays the melody over guzheng runs (slot `02`) on the chords.
3. **Build**: the dagu comes in on every beat, tanggu fills, the pipa lun (`04`) on long notes.
4. **Climax**: the dizi doubles the erhu an octave up; big gong on the first beat, bo cymbals.
5. **End**: back to the guqin, and one last big gong left to ring out.

Keep the drums on their own tracks, and give the erhu or dizi a whole track so their slides and vibrato are never cut by another sound.
