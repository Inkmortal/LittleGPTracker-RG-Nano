# Demo Songs

A handful of finished songs to open, play and pull apart: the walkthrough song you can build yourself, a funk / city pop feature tour, synthwave, a wuxia epic on real Chinese instruments, chiptune, house, and a stress test that runs every engine at once. Open them from the project list (they're in `Applications/Tracks` after [Install](Install)), press **Start** on the Song screen, then go poke around. Every idea in the other wiki pages is used somewhere in here.

<p>
<img src="images/demo-song-playing.png" width="240" alt="Neon Drive playing">
<img src="images/demo-phrase-melody.png" width="240" alt="Neon Drive lead phrase">
<img src="images/demo-mixer.png" width="240" alt="Mixer during playback">
</p>

## Afterglow — the walkthrough song

`lgpt_Afterglow` · C minor · 128 BPM with swing · 1:23

The melodic house track [Your First Song](Your-First-Song) builds press by press, finished. It starts from the synth kit every new song has and ends up using nearly everything the app does.

| Track | Part |
| --- | --- |
| 1 | Macro Synth kick, four on the floor; it drops out for the last bar of each build |
| 2 | Macro Synth snare with a reverb send; a `ROLL` + `VOLM` roll into each drop |
| 3 | Macro Synth hats, 16ths made with the fill tool, every 4th one `CHNC 0080` |
| 4 | offbeat bass between the kicks (the starter kit's `bass`, `sustain 00` for short notes) |
| 5 | HyperSynth `hyper pad`, chord `maj9` with `scale on`, `sub 00` and lows cut with its EQ, ducked by the kick (MOD `trig`) |
| 6 | FM4 `epiano` stabs on the offbeats with the bass, `CHRD 007E` |
| 7 | HyperSynth `trance lead`: the hook |
| 8 | a reversed crash from the `drums-909` pack, and one bar of the pad rendered to a sample and reversed |

**Look at:**
- **One bar, four chords** — the bass, pad and keys chains play the same phrase four times with transposes `00 FC 03 FE`: Cm9, Abmaj9, Ebmaj9, Bb9.
- **Song rows 00–0A** — intro, groove, build, drop, drop, break, build, drop, drop, outro, outro. Every section is the drop with parts taken out: chain `07` is the rest (a `KILL`), on every cell where a part drops out.
- **The low end** — the kick plays on the beat, the bass only between the kicks with short notes, and nothing else has any bass in it: that's why the drop is punchy instead of muddy.
- **The Groove** — `07 05` swings the 16ths.
- **Mix** — Mixer (kick up, bass down), FX reverb size, master EQ, limiter, and Project `Drive: 70` for headroom.

## Joyride — funk / city pop, the feature tour

`lgpt_Joyride` · C major (the last choruses in D) · 112 BPM with swing · 2:17

A fun one with a video-game soundtrack feel, and a tour of the whole app: the new drum and physical-modelling engines, FM, the HyperSynth, a wavetable chip lead, two samples from the packs, commands and a table, the effects and the mix. Every part is one small idea you can copy into your own songs.

| Track | Part | What it shows |
| --- | --- | --- |
| 1 | kick, and the tom fills | `drum` engine: `808 kick` (decay shortened so it's deep but tight), `808 tom` walking down at the end of each chorus |
| 2 | snare | `drum` engine `909 snare`: ghost notes are quiet `VOLM`s between the backbeats; a `VOLM 0030` + `ROLL 0092` roll swells into every chorus; reverb send |
| 3 | hats | `808 hat` 16ths with accents; the in-between ghosts have `CHNC 00B0`, so they play most of the time; an `808 open` hat on step `E` with `NTH 0044` (the 4th pass only), cut short by the next closed hat |
| 4 | bass | FM4 `slap bass`: a funk line with octave pops, ghost notes and `KILL`s, **one bar** that the chains transpose into every chord |
| 5 | pad | HyperSynth `strings`, chord `maj7` with `scale on`: one note a bar becomes the whole diatonic chord. Its own EQ cuts the lows. In the break a **table** gates it (`TABL 0000` + `TICK 0006`) |
| 6 | keys | FM4 `epiano` comping, `CHRD` voicings that move smoothly from chord to chord |
| 7 | lead | wav `chip lead` (pulse wave, vibrato, glide, dotted-8th echo): the hook, with `LEGA` slides, a `PTCH` bend and `VIBR` on the long notes; in the break a phys `vibes` plays it slowly; in the build an `ARPG` climbs |
| 8 | colour | phys `kalimba` arpeggios through the echo, a **trumpet sample** (`brass` pack) for the horn stabs (a `CHRD` triad on each hit), a **riser sample** (`textures` pack) into every chorus |

**What to listen for:**
- **Two chord progressions from one bar each.** The verse is I–vi–ii–V (Cmaj7, Am7, Dm7, G7), the chorus the "royal road" IV–V–iii–vi (Fmaj7, G7, Em7, Am7) that city pop and anime songs love. The bass and pad chains play one phrase four times with transposes: `00 09 02 07` and `05 07 04 09`.
- **The key change.** The last two choruses go up a whole tone: those chains transpose everything by 2 more, and a `SCAL 0215` on the pad moves the song's key to D so the HyperSynth's `scale on` chords follow. The intro's `SCAL 0015` puts it back in C.
- **The song gets bigger and smaller.** Intro (kalimba, pad, a tom run), groove, verse (the lead softer, `VOLM` on every note), pre-chorus (riser + snare roll), chorus (pad, horns, the hook on top), break (drums out, gated pad, vibes), build, two choruses in D, outro. Compare the loudness of each row.
- **The mix.** Mixer: keys down (`A8`), track 8 up (`E0`). Sends: reverb on the snare, pad and kalimba, the echo (`delay steps 3`, dotted 8ths) on the lead and kalimba, chorus on the pad. Master EQ adds a little weight and sparkle, the limiter catches the peaks, Project `Drive: 62` leaves headroom for eight tracks.
- **Swing** — groove `07 05`.

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
| 3 | temple block in the verses, pipa plucks in the second chorus |
| 4 | sub bass on the chord roots (synth) |
| 5 | soft string pad (synth) |
| 6 | guzheng — flowing broken chords and glissandos |
| 7 | erhu — the chorus melody |
| 8 | dizi flute in the verses, the big opera gong on section starts |

**Look at:**
- **Every melody uses only A C D E G** — the pentatonic scale is what makes it sound Chinese.
- **Sample instruments** — open any of them: each plays one recording, repitched from its `root` note. Erhu and dizi have two recordings each (low and high) so no note is stretched too far.
- **Grace notes** — a quick step into a melody note (`D4 E4` at the start of a bar) is how erhu and dizi players ornament.
- **Soft second plucks** — in the break the guzheng plucks its long notes again with `VOLM 50`, like a player letting the string ring. (Fast `RTRG` on a recorded instrument stutters, so it's saved for drums.)
- **Glissando** — a fast pentatonic run with rising `VOLM` before the chorus.
- **Opera percussion on one track** — each step picks its own instrument (clapper, small gong, cymbals).

The recordings are CC0 and CC BY 4.0 from Freesound; `CREDITS.md` in the song folder lists every author.

## Pixel Quest — chiptune

`lgpt_PixelQuest` · C major · 140 BPM · 1:02

**Look at:**
- **`ARPG 0047` / `0037`** — one held note becomes a buzzing 8-bit chord.
- **Drums from basic waves** — a triangle with a pitch drop for the kick, crunchy noise for snare and hats.
- **Echo lead** — track 7 plays the same melody with `DLAY 0003` and a delay send: a cheap stereo double.

## Engine Room — the stress test

`lgpt_EngineRoom` · A minor · 100 BPM · 0:38 loop

Every heavy sound at once, on all 8 tracks the whole time: Macro Synth drums, FM4 bass, a HyperSynth pad (a six-note chord of detuned saws), FM4 e-piano chords (`CHRD` makes four FM voices per note), a HyperSynth lead and sampled piano, with chorus, echo, reverb, the master EQ and the limiter all working. If this plays cleanly, anything you write will. The device log's `[HEARTBEAT]` lines show the audio engine's `load peak` while it plays.

## Sunset Club — house

`lgpt_SunsetClub` · A minor · 124 BPM · 1:02

Played on the [sample packs](Samples#sample-packs): 909 drums, piano chords and a finger bass.

**Look at:**
- **Four on the floor** — the kick on every beat (steps `0 4 8 C`), the clap on 2 and 4, the open hat on every offbeat (`2 6 A E`).
- **House piano** — one-shot min7/maj7 chords chopped into syncopated stabs, each cut short with `KILL`.
- **The bass between the kicks** — it plays the offbeats, so bass and kick never fight.

## Make them yours

- Change the tempo on the Project screen.
- Swap a preset: go to any instrument, **A + Left/Right** on `preset`.
- Mute tracks with **B + RB** to hear how each part is built.
- **Save Song As** to keep your version and leave the original untouched.

The songs are generated from Python in `tools/demos/` with the [song composer](Developer-Guide#song-composer), so they double as code examples of every technique.
