# Instrument-first workflow: research and proposals

Status: research only (2026-09-26). Nothing here is built; any of it changes the
app's workflow and needs the user's go-ahead first.

The ask: "I like to assemble my instruments first, then compose. Is there a way
to do that easily?"

## 1. How other trackers do it

| Tracker | Where you build sounds | Audition while building | Reuse across songs | Hand-off to composing |
| --- | --- | --- | --- | --- |
| **Dirtywave M8** | Instrument view per slot (128 slots). **Instrument Pool** view lists every slot with mixer columns; `EDIT` on an empty slot loads an instrument or a sample straight away | `EDIT+PLAY` previews the instrument (Instrument view); `PLAY` previews the selected slot in the Pool when stopped; `PLAY` previews songs, samples **and instruments** while browsing files | `LOAD`/`SAVE` fields on every instrument (`.m8i` preset files on the SD card). **SONG TEMPLATE** (Project/System view) makes the current song the start of every new song, so a favourite kit is always there | From the Pool, `SHIFT+LEFT` goes to the Phrase view **and sets the default inserted instrument** to the highlighted slot |
| **Polyend Tracker** | Load samples into the project's sample pool first (file browser), then build up to 48 instruments from them | 48-pad grid plays the instrument chromatically; file browser previews | Samples on the card; instruments per project | Pads also enter notes into the pattern |
| **LSDJ** | Instrument screen (pulse/wave/kit/noise) | Play the note in a phrase | Kits (drum sample banks) patched into the ROM; songs as templates | Phrase instrument column |
| **Renoise** | Instrument box + sampler/plug-in editors | **Keyjazz**: the computer keyboard plays the selected instrument across two octaves at any time; disk browser auto-previews | `.xrni` instrument files, song templates | The selected instrument is what new notes get |
| **Furnace / Deflemask** | Instrument list + editor | Keyjazz on the computer keyboard | `.fui` / `.dmp` instrument files | The selected instrument is what new notes get |
| **SunVox** | Modules wired in a modular view | Keyjazz on keyboard/touch | Module/instrument files, templates | Selected module gets the notes |

### What users say

- Presets and templates are the main instrument-first tool on the M8: users keep a
  template song with their kit and sell or share instrument preset packs (drum kits
  made from the synth engines, bass packs). The template trick (`/System/TEMPLATE.m8s`,
  now a Project setting) is one of the most-shared M8 tips.
- A common complaint about hardware trackers is how many menus it takes to set up
  instruments. MusicRadar on the Polyend Tracker: "loading samples and setting up
  instruments does involve jumping between a fair few menus".
- Not being able to hear a sound from the file picker is a known annoyance (Furnace
  issue #708: users expect to highlight an instrument file and press a key to hear it,
  "like many other trackers").
- Digitone owners asked for live preview of presets while browsing (Elektronauts), and
  M8 users welcomed the 4.0 Instrument Pool for the same reason: seeing and hearing
  every sound in one place.
- Composers who design sounds after writing parts report it as the slow, painful half.
  Some producers spend whole sessions only making sounds and saving them for later.

The pattern that works everywhere has four parts:
1. **One place that shows every sound.**
2. **Hear any sound instantly**, ideally playing notes on it (keyjazz), not just one
   fixed note.
3. **Save and load sounds and kits** so the work carries over to the next song.
4. **The sound you picked is the one your next note uses.**

## 2. This app today

Code read for this: `InstrumentView*.cpp`,
`ModalDialogs/InstrumentListDialog.cpp`, `ImportSampleDialog.cpp`,
`InstrumentBank.cpp` (starter kit), `SynthInstrument.cpp` (presets),
`PhraseView.cpp`, `ScaleView.cpp` and `docs/rgnano-wiki/Controls.md`.

What exists:
- **Starter kit:** a new song with no samples gets 16 synth presets in slots 00-0F
  (kick, snare, hat, openhat, clap, bass, lead, pad, pluck, keys, bell, acid, subbass,
  chip, tom, perc).
- **Presets:** synth and macro presets are compiled into the app. On the SOUND page,
  A+Left/Right on `preset` browses them within the chosen `engine`.
- **Instrument list:** a dialog opened with RB+Up on the Instrument screen. It shows all
  128 slots with type, name and usage count, plus a waveform preview. Start plays the
  selected sound at one note, Select renames, LB+A copies to a free slot, and A opens
  the slot.
- **Sample browser:** Start+Up/Down previews while moving.
- **Audition:** A+Start plays the instrument at C3 (samples at their root note).
  RB+A+Left/Right plays an octave down or up. The Scale screen also has a
  keyboard that plays the current instrument on any note (`ScaleView::hearNote`).

What gets in the way of building instruments first:
1. **No route to the sounds without composing first.** The Instrument screen is only
   reachable from a phrase (RB+Right on the Phrase screen). From a new song that means
   Song A → RB+Right → Chain A → RB+Right → RB+Right: 5 presses, which leave behind a
   chain and a phrase you didn't ask for.
2. **You hear one note, not a playable instrument.** A bass or lead can't be judged
   from a single C3. There is no keyjazz on the Instrument screen or in the list.
3. **Browsing presets is silent.** Each A+Left/Right step on `preset` needs a separate
   A+Start to hear it, so trying 5 presets takes about 10 presses.
4. **Nothing carries to the next song.** There are no user presets, instrument files,
   kits or song template. Only the built-in starter kit.
5. **The hand-off loses your pick.** A new note in a phrase uses `PhraseView::lastInstr_`
   (the last instrument touched in that phrase), not the instrument you were just
   building. After making a bass in slot 05, the first note you enter on the Phrase
   screen still gets instrument 00 until you change the `I` column.

Key presses to build a 4-instrument kit from presets today, counted from the key map
and the code (not a sim run). Assumes you try about 5 presets per slot and hear each:

| Step | Presses |
| --- | --- |
| New song → Instrument 00 (via chain + phrase) | 5 |
| Per slot: reach `preset` (1-2), 5 × (A+Right, A+Start) = 10, next slot B+Right (1) | ~13 |
| 4 slots | ~52 |
| Name each (RB+Up, Select, type ~6 letters at ~2-3 moves each, Start, B) | ~20 per slot |
| **Total** | **~57 without names, ~135 with names** |

For sample instruments, each slot also needs: type → sample (2), open the browser (2),
find the pack folder (3-5) and preview with Start+Down. That's roughly 15-20 presses
per slot, before any shaping.

## 3. Proposals

The screen is 30×30 characters (8 px font on 240×240). The mockups use that grid.

### Option A: Sound Rack screen (recommended)

The instrument list becomes a real screen you reach straight from the Song, where you
**play** the selected sound, browse presets/samples/your saved sounds with instant
preview, and jump into editing and back. It's M8's Instrument Pool plus keyjazz.

Where: **Song RB+Left** (unused today) opens the Rack. RB+Right edits the selected
slot, and RB+Left on the Instrument screen returns to the Rack if you came from it
(otherwise to the Phrase, as now).

```
 RACK            kit: Dusk  05
 00 SYN kick      ■■■   2
 01 SYN snare     ■■■   2
 02 SMP hat-909   ■■    1
 03 SYN openhat
 04 SYN clap
>05 SYN acid bass  ▶  <- playing
 06 SYN lead
 07 SMP chord-ep
 08 --- (empty: SEL browse)
 09 ---
 0A ---
 0B ---
 ┌──────────────────────────┐
 │  ~~~ waveform ~~~        │
 └──────────────────────────┘
 ┌┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┐
 ││█││ ││█││█││ ││█││ ││█││ │  keyboard
 └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┘
   C3      ^ E3       (in key)
 A play  A+◄► note  A+▲▼ oct
 SEL browse  RB+► edit
```

Keys (these follow the existing combo rules in Controls.md):

| Input | Does |
| --- | --- |
| Up/Down | pick a slot (Left/Right: a page, as in every list) |
| **A** (hold) | play the slot at the keyboard note; release stops it (like A on a phrase note) |
| **A + Left/Right** | move the keyboard note a scale step (song key/scale) and play it |
| **A + Up/Down** | an octave down/up and play |
| **Start** | play a short demo riff for this kind of sound (a drum hit pattern, a bass line, a chord), looping, so it's heard in a musical context; again stops |
| **Select** | **browse**: presets of every engine, sample packs and your saved sounds in one list that previews as you move. A puts it in the slot |
| **RB + Right** | edit the slot on the Instrument screen (RB+Left there comes back) |
| **RB + Down** | **compose with it**: go to the Phrase (making a chain and phrase if the song has none) with this slot as the instrument for new notes |
| **LB + A** | copy to a free slot (existing) |
| **B + A** | clear the slot |
| **LB + Start** | kit menu: save kit, load kit, set as new-song template |
| **B** | back to the Song |

What it reuses:
- `InstrumentListDialog`: rows, usage counts, waveform preview, copy and rename move
  into a View.
- `ScaleView::hearNote` and its keyboard drawing for the keyjazz strip.
- `Player::AuditionInstrument` for preview.
- `ImportSampleDialog` preview code for the browser.
- `SynthInstrument`/`MacroInstrument::LoadPreset` for preset entries.
- `InstrumentBank::SaveContent`/`RestoreParam` to write and read single instruments
  and kits as the same `<INSTRUMENT><PARAM/>` XML the song file already uses.

New pieces:
- A `VT_RACK` view type with its helper overlay page and guide section.
- An instrument/kit file format (`Tracks/../Instruments/*.lgi`, `Kits/*.lgk`), using
  the song file's XML fragments. Sample instruments store the sample's path and import
  it into the song's pool on load. The instrument's table is saved with it.
- A unified browser list (presets + samples + saved sounds) with preview on move.
- A hand-off hook: `PhraseView` takes `viewData_->currentInstrument_` as `lastInstr_`
  when entered from the Rack or Instrument screen, so the next note uses the sound you
  built. This also fixes gap 5 for everyone.
- Demo riffs: 3-4 built-in 1-bar patterns (drums/bass/chord/lead) played through the
  audition channel.

Effort: medium-large. It's one new screen, a file format and a browser. Everything
else is reuse. It needs sim tests for each key and a guide page.

Same 4-instrument kit with the Rack: Song RB+Left (1), then per slot Select (1),
move through presets hearing each as you move (5), A to take it (1), then A+◄► to
try it across notes (2-4), Down to the next slot (1). That's about **9-12 per slot,
~45 total**, with every sound played across a range, not just at C3. Starting the
next song from your kit template takes **0 presses**.

Trade-offs: another top-level screen to learn (mitigated by the map and helper). Demo
riffs need care to sound good. Kit files add card clutter (one folder).

### Option B: Kits and templates, no new screen

Keep the current screens and add the reuse pieces:
- **Instrument screen:** `load` / `save` fields on the first page, as on the M8. They
  read and write `.lgi` files, and the load browser previews on move.
- **Instrument list:** LB+Start saves the whole list as a kit and loads a kit. A
  project setting "new songs start from this kit" replaces the starter kit with your own.
- **Browse sound:** `preset` browsing plays each preset as you step (auto-audition,
  stopping the previous one).
- **Same hand-off fix** as Option A.
- **Direct route:** Song RB+Left opens the Instrument screen for the last instrument,
  with no chain or phrase needed.

```
 INSTRUMENT 05  SOUND   1/7
 type      synth
 engine    analog
 preset    acid       ♪ plays as you step
 load      ...        A: pick a saved sound
 save      ...        A: save as "acid bass"
 wave      saw
 ...
```

Reuses: all of Option A's file and hand-off work, minus the Rack view. Effort: small-medium.
Trade-offs: still only one note to judge a sound by, and the list stays a dialog. It
fixes carry-over and silent browsing but not "play my sounds before writing notes".

### Option C: Jam pad (keyjazz + record)

A performance screen where the D-pad plays the four in-scale notes around a root, LB/RB
shift octave or root, and B+direction switches between 4 pinned instruments. Start
records what you play into the current phrase, quantized to steps. It's the Polyend-pad
feel on 10 buttons.

```
 JAM  C minor   oct 3
          ▲ Eb
    ◄ C       G ►
          ▼ Bb
 inst: 05 acid bass   [B+dir]
 pinned: 00 kick 01 snare
         05 bass  07 chord
 LB/RB oct   START rec
```

Reuses: the audition path, scale snapping (`snapToScale`) and phrase writing. Effort:
medium, and it overlaps a lot with Option A's keyjazz. Trade-offs: great for trying
melodies, but it's a composing tool more than an instrument-assembling one. It
doesn't address reuse across songs. Combos needed for 4+ notes clash with the "A
is press/confirm" rule.

## Recommendation

**Option A, built in milestones, starting with the parts it shares with B:**

1. Hand-off fix: new phrase notes use the instrument you last built or picked. Small,
   fixes a real annoyance today.
2. Instrument/kit files, plus "new songs start from my kit" (M8's template, which users
   rely on most).
3. The Rack screen at Song RB+Left: list, hold-A keyjazz with A+◄►/▲▼, and the browser
   with preview on move.
4. Demo riffs on Start, and RB+Down "compose with it".

Each milestone is useful alone, so the user can test on the Nano between steps. Option
C's pad could later become a mode of the Rack keyboard if playing melodies proves to be
the main need.

## Sources

- [Dirtywave M8 Operation Manual 6.0.0](https://images.equipboard.com/uploads/item/manual/136211/dirtywave-m8-tracker-model-02-manual.pdf): Instrument View (p.16-17, LOAD/SAVE, EDIT+PLAY preview), Instrument Pool View (p.22-23, preview with PLAY, SHIFT+LEFT sets default instrument), file browser preview, SONG TEMPLATE
- [DirtyWave M8 Tips](https://github.com/pauley-unsaturated/DirtyWave-M8-Tips): template song, building instruments and tables before sequencing
- [Dirtywave M8 changelog](https://github.com/Dirtywave/M8Firmware/blob/main/changelog.txt)
- [DW01 Synthdrums preset pack](https://dirtywave.com/products/dw01-synthdrums), [M8 presets on Patchstorage](https://patchstorage.com/platform/dirtywave-m8/), [M8 Resources, nora's notes](https://notes.nora.codes/m8-resources/)
- [Elektronauts M8 thread](https://www.elektronauts.com/t/m8-tracker/125103?page=372), [Digitone: live preview of presets while browsing](https://www.elektronauts.com/t/live-preview-of-presets-while-browsing/148932)
- [MusicRadar: Polyend Tracker review](https://www.musicradar.com/reviews/polyend-tracker), [Sound On Sound: Polyend Tracker](https://www.soundonsound.com/reviews/polyend-tracker), [Engadget: Polyend Tracker review](https://www.engadget.com/polyend-tracker-groovebox-sampler-review-170118746.html)
- [LSDJ Custom Kits HOWTO](https://littlesounddj.fandom.com/wiki/Custom_Kits_HOWTO), [LSDJ 3.7.4 manual](https://www.littlesounddj.com/lsd/latest/documentation/LSDj_3_7_4.pdf)
- [Renoise: Playing notes with the computer keyboard](https://tutorials.renoise.com/wiki/Playing_Notes_with_the_Computer_Keyboard), [Renoise Disk Browser](https://tutorials.renoise.com/wiki/Disk_Browser), [Renoise Instrument Selector](https://tutorials.renoise.com/wiki/Instrument_Selector)
- [Furnace instruments doc](https://github.com/tildearrow/furnace/blob/master/doc/4-instrument/README.md), [Furnace issue #708: can't preview instruments from the file selector](https://github.com/tildearrow/furnace/issues/708)
- [SunVox manual](https://warmplace.ru/soft/sunvox/manual.php)
- [Mars After Midnight devlog: sound design after composing](https://dukope.itch.io/mars-after-midnight/devlog/506740/sound-design)
