# Demo Songs

Four finished songs built only from the synth kit. Open them from the project list (they're in `Applications/Tracks` after [Install](Install)), press **Start** on the Song screen, then go poke around. Every idea in the other wiki pages is used somewhere in here.

<p>
<img src="images/demo-song-playing.png" width="240" alt="Neon Drive playing">
<img src="images/demo-phrase-melody.png" width="240" alt="Neon Drive lead phrase">
<img src="images/demo-mixer.png" width="240" alt="Mixer during playback">
</p>

## Neon Drive — synthwave

`lgpt_NeonDrive` · A minor · 100 BPM · 1:55

| Track | Part |
| --- | --- |
| 1 | kick (half-time verses, four-on-the-floor choruses) |
| 2 | snare with reverb, fill at the end of every 4 bars |
| 3 | hats, accented 16ths in the chorus |
| 4 | octave-jumping 8th-note bass |
| 5 | pad chords, one note + `CHRD` per bar |
| 6 | pluck arpeggio |
| 7 | lead melody (a bell takes it in the break) |
| 8 | crash and tom fills |

**Look at:**
- **Song rows 00–0B** — each row is a section: intro, verse, chorus, break, build, outro.
- **Track 6 in the intro** — only the first note has an instrument number, so the `FCUT 60C0` filter sweep keeps opening across four bars.
- **Track 5** — chords are voiced as inversions (`C 3` + `CHRD 0059`), so they move smoothly instead of jumping.
- **The build** — a snare crescendo with `VOLM`, then `RTRG 0003` for a 32nd-note roll into the drop.

## Jade Sword — wuxia / donghua

`lgpt_JadeSword` · A minor pentatonic · 92 BPM · 2:05

Neon Drive's song map re-scored for a martial-arts epic, played on **recordings of real Chinese instruments**. Compare the two projects screen by screen.

| Track | Part |
| --- | --- |
| 1 | dagu (big drum) with rim hits |
| 2 | Beijing-opera percussion: bangu clapper drum, small gong, cymbals |
| 3 | temple block in the verses, pipa tremolo in the second chorus |
| 4 | sub bass on the chord roots (synth) |
| 5 | soft string pad (synth) |
| 6 | guzheng — flowing broken chords, glissandos, tremolo |
| 7 | erhu — the chorus melody |
| 8 | dizi flute in the verses, the big opera gong on section starts |

**Look at:**
- **Every melody uses only A C D E G** — the pentatonic scale is what makes it sound Chinese.
- **Sample instruments** — open any of them: each plays one recording, repitched from its `root` note. Erhu and dizi have two recordings each (low and high) so no note is stretched too far.
- **Grace notes** — a quick step into a melody note (`D4 E4` at the start of a bar) is how erhu and dizi players ornament.
- **Pipa and guzheng tremolo** — `RTRG 0002` retriggers the note every two ticks.
- **Glissando** — a fast pentatonic run with rising `VOLM` before the chorus.
- **Opera percussion on one track** — each step picks its own instrument (clapper, small gong, cymbals).

The recordings are CC0 and CC BY 4.0 from Freesound; `CREDITS.md` in the song folder lists every author.

## Rainy Window — lo-fi hip-hop

`lgpt_RainyWindow` · F major · 78 BPM with swing · 1:38

**Look at:**
- **Groove `07 05`** — every other 16th is late: that's the lazy swing.
- **Jazz chords** — `CHRD 037A` (min7), `047A` (dom7), `047B` (maj7) on the keys, re-hit on step `0A` at lower volume.
- **Vinyl hiss** — track 7 is a noise synth holding one very quiet note for the whole song.

## Pixel Quest — chiptune

`lgpt_PixelQuest` · C major · 140 BPM · 1:02

**Look at:**
- **`ARPG 0047` / `0037`** — one held note becomes a buzzing 8-bit chord.
- **Drums from basic waves** — a triangle with a pitch drop for the kick, crunchy noise for snare and hats.
- **Echo lead** — track 7 plays the same melody with `DLAY 0003` and a delay send: a cheap stereo double.

## Make them yours

- Change the tempo on the Project screen.
- Swap a preset: go to any instrument, **A + Left/Right** on `preset`.
- Mute tracks with **B + RB** to hear how each part is built.
- **Save Song As** to keep your version and leave the original untouched.

The songs are generated from Python in `tools/demos/` with the [song composer](Developer-Guide#song-composer), so they double as code examples of every technique.
