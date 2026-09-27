# RG Nano vs Dirtywave M8: capability audit

Rewritten 2026-09-27 against the code at `fdffa33` plus the work merged since, and the M8 at firmware 6.6 (Dirtywave manual 6.0.0 and the community EFX/sampler/FM/hypersynth/macrosynth references, see Sources). It replaces the 2026-09-21 audit, which listed EQ, limiter, Macro Synth, mod slots, Scale screen, render-to-sample, sample editing, FX screen and Live mode as missing. All of those exist now.

Status: **Have** = works, proven by code and a sim test. **Partial** = an equivalent exists but does less than the M8. **Missing** = not built. **Hardware** = the Nano can't do it (not a build gap). **In progress** = another agent is building it right now.

Proof is a file (`sources/...`) or a sim script (`projects/resources/RGNANO_SIM/<name>.rgsim`). Key paths use the Nano's names: LB/RB = shoulders, Select = FN.

## Short answer

The musical core is at M8 level: 8 tracks, 256-row song, 255 chains/phrases, 128 instruments, tables, grooves, Live mode, sampler with editor and slices, four synth engines that mirror the M8's (FM4, Hyper, Wav, Macro with the same 47 Braids models), four mod slots with the M8's modulator types, send FX, master EQ, limiter and render-to-sample. Where the Nano is behind is **depth, not breadth**: 2 FX columns instead of 3 and no velocity column; about half of the M8's 3-letter commands, and almost none of its per-parameter instrument/mixer commands; simpler send FX; no wavetables in the Wav engine; a smaller sample editor; no MIDI; no instrument/kit files yet (in progress).

Counts (rows in the tables below): **Have 79 · Partial 32 · Missing 47 · Hardware 2 (MIDI) · In progress 1 (kits/instrument files; the pool counts as Partial) · Nano-only extras 6**. Most of the Missing rows are single commands; the screens and engines are nearly all Have or Partial.

## Hardware: what the Nano can't match

| Area | M8 | RG Nano | Consequence |
| --- | --- | --- | --- |
| CPU | Teensy 4.1, 600 MHz Cortex-M7, bare metal, nothing else running | V3s, 1.2 GHz Cortex-A7, single core, Linux + SDL | Similar raw budget; ours pays for the OS and the screen. Heavy engines (Hyper, FM4 feedback, Macro) must be costed per voice (tools/dsp-harness) |
| Memory | Sample RAM of the M8 Model 02 | ~40 MB usable heap of 64 MB (sim enforces it, `RGNanoSimMemory.cpp`) | Plenty for a song; very long samples are refused cleanly (`sample-too-long.rgsim`) |
| Audio I/O | Line in/out, headphone, USB audio in/out, built-in mic on some models | Speaker + USB-C; input needs a UAC USB adapter and a kernel with USB audio (README_AUDIO_INPUT.md) | Recording and live input are optional extras, not the default |
| MIDI | TRS MIDI in/out + USB MIDI | No ports; USB-C OTG host is possible but not in the firmware | MIDI features are a firmware project, not an app one |
| Screen / keys | 320x240, 8 keys (arrows, Shift, Play, Option, Edit) | 240x240, D-pad + A B X Y + LB RB + Start Select + Power | We have **more** buttons. X and Y are copy and paste on every screen (**X** copy, **Y** paste, **LB + Y** paste new copies; `View::CopyAtCursor`/`PasteAtCursor`, `copy-paste-xy.rgsim`) |

So nothing the M8 does musically is out of reach for the hardware except MIDI and a built-in line in. The rest is build work.

## Screens

| M8 screen | Nano | Status | Proof | Nano key path |
| --- | --- | --- | --- | --- |
| Song | Song | Have | `SongView.cpp`, `song-tools-create.rgsim` | start screen |
| Live mode | Song → Select | Have | `live-mode.rgsim` | Select (1) |
| Chain | Chain | Have | `ChainView.cpp`, `chain-warp.rgsim` | RB+Right (1) |
| Phrase | Phrase | Partial: 2 FX columns, no velocity column | `PhraseView.cpp` | RB+Right ×2 |
| Instrument | Instrument (synth / sample / macro / MIDI) | Have | `synth-instrument-pages.rgsim`, `sample-screen-full-audit.rgsim` | RB+Right ×3 |
| Instrument modulation | MOD page, 4 slots | Have | `InstrumentViewMod.cpp`, `mod-slots.rgsim` | LB+Right on Instrument |
| Instrument pool | Instrument list (RB+Up) | Partial → In progress (Sound Rack) | `InstrumentListDialog.cpp`, `instrument-list.rgsim`; docs/INSTRUMENT_FIRST_WORKFLOW.md | RB+Up from Instrument |
| Table | Table | Partial: 128 tables (M8 256), no TIC map modes | `TableView.cpp`, `sequencer-commands.rgsim` | RB+Down from Phrase |
| Groove | Groove | Have (32 grooves) | `GrooveView.cpp`, `command-runtime-workflow.rgsim` | RB+Up from Phrase |
| Scale | Scale | Have (keys, all scales, custom scale) | `ScaleView.cpp`, `scale-editor.rgsim` | Project → RB+Right |
| Mixer | Mixer | Partial: no input strip, no DJ filter | `MixerView.cpp`, `mixer-levels.rgsim` | Song → RB+Down |
| Effect settings | FX | Partial (fewer knobs, see Send FX) | `FXView.cpp`, `fx-screen.rgsim` | Mixer → RB+Down |
| EQ editor | EQ (master) + EQ page per instrument | Have | `EQView.cpp`, `eq-screen.rgsim`, `instrument-eq.rgsim` | FX → RB+Right |
| Limiter & mix scope | Limit | Have | `LimiterView.cpp`, `master-limiter.rgsim` | EQ → RB+Right |
| Project | Project | Have | `ProjectView.cpp` | Song → RB+Up |
| Render | Project → Render | Partial: stereo/stems, no range/name | `render-export-workflow.rgsim` | Project, Render field |
| Selection to sample (quick render) | LB+Start on Phrase / Chain | Partial: bar or chain, not a song selection | `render-to-sample.rgsim` | LB+Start (1) |
| Sampler | Sample instrument, 7 pages | Have | `sample-lab-pages-preview.rgsim` | Instrument, type sample |
| Sample editor | Select on a sample page | Partial (6 of ~16 processes, no markers) | `SampleEditDialog.cpp`, `sample-processing.rgsim` | Select (1) |
| Sample browser | Import dialog with preview | Have | `ImportSampleDialog.cpp`, `sample-import-workflow.rgsim` | A on `sample` |
| Recording | Record dialog | Partial: USB adapter + custom kernel only; no track/resample sources | `RecordSampleDialog.cpp` | Import → Record |
| Effect command help | Command picker + helper page | Have | `CommandSelectorModal.cpp`, `command-selector-workflow.rgsim` | Select on a command |
| System settings | Power menu (volume, brightness, save/quit, debug) | Partial | `SDLEventManager.cpp` power menu, `power-menu-input-isolation.rgsim` | Power |
| Theme | none | Missing | | |
| MIDI settings / mapping | none on the Nano | Hardware | `DummyMidi` in the RGNANO build | |
| Time stats | heartbeat log, debug tools | Partial | `CrashLog.cpp`, Debug tools | Power → Debug tools |
| Snapshots (save/recall song state) | none | Missing | | |
| — (Nano only) Helper overlay | RB+Select on every screen | Nano extra | `context-overlay-all-screens.rgsim` | RB+Select |
| — (Nano only) Built-in guide | from the helper or start screen | Nano extra | `help-topics.rgsim` | helper → A |
| — (Nano only) Undo/redo, 32 steps | B+Select / LB+Select | Nano extra | `undo.rgsim` | 1 press |

## Instrument engines

| M8 | Nano | Status | Proof |
| --- | --- | --- | --- |
| Wavsynth: 9 basic shapes, size, mult, warp, scan/mirror | `wav` engine: same 9 shapes and knobs, drive + limit | Have | Synth.md "WAV engine", `synth-engines.rgsim` |
| Wavsynth wavetables 09–45 (scan through 64 waves) | none in `wav` (Macro models 25–28 cover some) | Missing |  |
| Wavsynth WAV LP/HP/BP/BS 8-bit filters | none | Missing |  |
| Macrosynth: 47 Braids models, timbre, color | Macro synth, the same 47 models | Have | `MacroInstrument.cpp`, `macro-synth.rgsim` |
| Macrosynth DEGRADE / REDUX, TRG re-strike | drive only | Missing |  |
| Sampler: play modes incl. ping-pong, OSC | 10 play modes incl. osc and loop-sync | Have | Samples.md "Play modes", `looping-sample-stops.rgsim` |
| Sampler REPITCH / BPM (loop follows tempo) | `loop-sync` mode | Partial (no STEPS/BPM field) | Samples.md |
| Sampler start / loop start / length / detune / degrade | start, loop, end markers, fine, crush | Have | `instrument-start-trim-preview.rgsim` |
| Sampler slices: equal, from file markers, 128 max | equal slices only (`slices`) | Partial | `SampleInstrument.cpp:358` |
| FM: 4 ops, 12 algorithms, ratio, level, feedback, 12 shapes | `fm4`: 4 ops, 12 algorithms, ratio, level, fbk, 12 shapes, **plus a per-operator envelope the M8 lacks** | Have | Synth.md "FM4 engine", `synth-engines.rgsim` |
| FM op-mods (MOD1–4 routed to LEV/RAT/PIT/FBK per op) | MOD slot → `fm amt` (all modulators together) | Partial |  |
| FM wavetable shapes (W…) | none | Missing |  |
| Hypersynth: 6 notes, 16 chord banks, shift, swarm, width, sub, scale | `hyper`: 6 notes, 15 chord presets + custom, shift, swarm, width, sub, scale | Have | Synth.md "HYPER engine" |
| Hypersynth 12 oscillator shapes | saw only | Partial |  |
| MIDI Out instrument | MIDI instrument type exists, no port on the Nano | Hardware | `MidiInstrument.cpp` |
| External instrument (audio in through an instrument) | none | Missing (and needs the input hardware) |  |
| Per-instrument filter types (LP, HP, BP, BS, LP>HP, …) | lowpass / highpass / bandpass / off | Partial (no notch/combined) | `SynthInstrument.cpp:42` |
| Per-instrument AMP + LIM (clip, sin, fold, wrap, post) | drive + limit (soft, clip, sin, fold, wrap) | Have | Synth.md "WAV engine" `limit` |
| Per-instrument sends: mod FX, delay, reverb | chorus, delay, reverb sends on every type (samples too) | Have | `SampleInstrument.cpp:1240` |
| Per-instrument EQ | EQ page on every type | Have | `instrument-eq.rgsim` |
| Instrument save/load, kits, templates | none | In progress (Sound Rack step 2) | docs/INSTRUMENT_FIRST_WORKFLOW.md |
| — (Nano only) the original LGPT synth with 2-op FM + chords, and a 16-sound starter kit | | Nano extra | `synth-starter-kit.rgsim` |

## Modulation

| M8 | Nano | Status | Proof |
| --- | --- | --- | --- |
| 4 mod slots per instrument | 4 slots | Have | `mod-slots.rgsim` |
| AHD envelope | `ahd` | Have | Synth.md "MOD" |
| ADSR envelope (release on OFF) | `adsr`, release on `KILL` | Have |  |
| Drum envelope | `drum` | Have |  |
| LFO: shapes tri, sin, ramp, exp, square, random, drunk; trig free/retrig/hold/once | all of those | Have |  |
| Trig envelope (fired by another track) | `trig` with `source` track | Have |  |
| Tracking (note / velocity → value) | `track` by note | Partial (no velocity source; there is no velocity column) |  |
| Destinations incl. sends, filter, pitch, engine params | volume, cutoff, reso, pitch, pan, fine, drive, engine params, sends | Have |  |
| Mod slot → another mod slot's amount / FM op-mods | none | Missing |  |

## Sequencer and phrase editing

| M8 | Nano | Status | Proof / keys |
| --- | --- | --- | --- |
| 256 song rows, 8 tracks | same | Have | `Song.h:8` |
| 255 chains, 16 rows, transpose column | same | Have | `Chain.h:4` |
| 255 phrases, 16 steps | same | Have | `Phrase.h:5` |
| Phrase columns: note, **velocity**, instrument, **3** FX | note, instrument, **2** FX | Partial | `PhraseView.cpp` |
| 256 tables | 128 | Partial | `Table.h:8` |
| 32 grooves | 32 | Have | `Groove.h:10` |
| Clone (new copy of a chain/phrase) | **Y** with nothing copied (new copy in the next empty row), **LB + Y** (paste new copies); B+LB then A+LB still works | Have (Controls.md "Copy and paste", helper) | `Song::DeepCloneChain`, `Phrase::Clone`, `copy-paste-xy.rgsim` |
| Deep clone (chain + its phrases) | Song **Y** / **LB + Y** always copy the phrases too; A+LB a second time on Song still works | Have (Controls.md "Copy and paste") | `Song::DeepClonePhrases`, `copy-paste-xy.rgsim` |
| Interpolate a column | selection, B+RB | Have (undocumented; B+RB is also mute outside selection) | `PhraseView.cpp:709`, `TableView.cpp:144` |
| Note fill / random fill / random notes | LB+Left / LB+Right on a selection; shuffle and reverse too | Have (more than the M8) | `random-tools.rgsim` |
| Copy / cut / paste selections | B+LB, B, A+LB | Have | Controls.md |
| Move selection (EDIT+UP/DOWN) | none | Missing |  |
| Insert row (SHIFT+EDIT) | none as a key; paste shifts rows down on the Song | Partial |  |
| Bookmarks | A+Select on Song, LB+Up/Down jumps | Have (one colour; M8 has colours) | `song-tools-create.rgsim` |
| Move track | Up on row 00 | Have | `song-tools-create.rgsim` |
| Mute / solo / unmute all | B+RB, A+RB, RB+LB | Have | `key-grammar.rgsim` |
| Cue row while playing | B+Start (jump now), LB+Start in Live | Have | Controls.md |
| Show track time | none | Missing |  |
| Render a song selection to a sample | bar / chain only | Partial | `render-to-sample.rgsim` |
| Capture FX command from a parameter (tap EDIT on a knob) | none | Missing |  |
| Quick FX jump / parameter map | none | Missing |  |
| Undo | 32 steps everywhere | Have (Nano extra) | `undo.rgsim` |

## FX commands

Nano commands are four letters. "Nano" names the equivalent.

### Sequencer and flow

| M8 | Does | Nano | Status |
| --- | --- | --- | --- |
| ARP | 3-note arpeggio | `ARPG` (4 notes) | Have |
| ARC | arp mode and speed | none | Missing |
| CHA | chance (per FX side) | `CHNC` (whole step) | Partial |
| DEL | delay the row | `DLAY` | Have |
| GRV | track groove | `GROV` | Have |
| GGR | groove on all tracks | none | Missing |
| HOP | hop to next phrase row / table row | `HOP` | Have |
| KIL | cut after ticks | `KILL` | Have |
| OFF | note off: start the ADSR release | `KILL` releases synths and `adsr` samples | Partial (one command for both) |
| INS | switch instrument | none (instrument column) | Missing |
| NXT | set instrument on the next track | none | Missing |
| RET | retrigger with volume ramp | `ROLL` (and `RTRG`) | Have |
| REP / RTO | step the previous FX each row | none | Missing |
| NTH | Nth trigger | `NTH` | Have |
| RND | randomize previous FX | `RAND` (the other command on the step) | Have |
| RNL | randomize the FX to the left / note+instrument | `RAND` alone moves the note | Partial |
| RMX | remix (per-track phrase row) | none | Missing |
| MTT | micro timing, 1/8 tick | none | Missing |
| ERR | randomize pitch and params | none | Missing |
| SCA | scale per track | `SCAL` (song-wide) | Partial |
| SCG | scale on all tracks | `SCAL` | Have |
| SED | seed | `SEED` | Have |
| PSL | pitch slide | `LEGA` | Have |
| PBN | continuous pitch bend | `PTCH` bends to a target | Partial |
| PVB / PVX | vibrato / extreme vibrato | `VIBR` | Partial (no PVX range) |
| TPO | tempo | `TMPO` | Have |
| TSP | global transpose | `TRSP` | Have |
| TBL | table select | `TABL` | Have |
| TIC | table tick rate | `TICK` | Have |
| TIC modes FC–FF (octave/velocity/note map, 200 Hz) | table row picked by note etc. | none | Missing |
| THO | table hop | `THOP` | Have |
| TBX | aux table beside the note | none | Missing |
| — | stop the song | `STOP` | Nano extra |

### Common instrument FX

| M8 | Nano | Status |
| --- | --- | --- |
| VOL | `VOLM` (absolute with ramp, M8 is relative) | Have |
| PIT | `PTCH` | Have |
| FIN | `PFIN` | Have |
| CUT / RES | `FCUT` / `FRES` (with speed), `FLTR` both at once | Have |
| FIL (filter type) | none | Missing |
| AMP / LIM | `CRSH` drive; limit mode not a command | Partial |
| PAN | `PAN` | Have |
| SMX / SDL / SRV (send amounts) | none (knobs and MOD slots only) | Missing |
| EA/AT/HO/DE/ET (envelope amount, times, retrigger per slot) | none | Missing |
| LA/LF/LT (LFO amount, rate, retrigger) | none | Missing |

### Engine-specific

| M8 | Nano | Status |
| --- | --- | --- |
| Sampler PLY | `PLAY` | Have |
| Sampler SLI | `SLCE` is listed in Commands.md and the picker but **nothing handles it** (no `I_CMD_SLCE` case in `SampleInstrument.cpp`) | Missing (doc bug) |
| Sampler STA | `PLOF` | Have |
| Sampler LOP | `LPOF` | Have |
| Sampler LEN | none | Missing |
| Macro OSC (model per step) | none | Missing |
| Macro TBR / COL | `TIMB` / `COLR` | Have |
| Macro DEG / RED | `CRSH` on samples only | Missing for macro |
| Macro TRG | none | Missing |
| FM ALG / FM1–4 | none | Missing |
| Wav OSC / SIZ / MUL / WRP / SCN | none | Missing |
| Hyper CRD | `CHRD` (sets the six notes) | Have |
| Hyper SWM / WID / SUB | none | Missing |
| — sample feedback (`FBMX`/`FBTN`), `IRTG` | | Nano extra |

### Mixer FX (global)

| M8 | Nano | Status |
| --- | --- | --- |
| VMV, VCH, VDE, VRE, VT1–VT8 (mixer levels per step) | none | Missing |
| XCM/XCF/XCW (chorus), XDT/XDF/XDW (delay), XRS/XRD/XRW (reverb) per step | none | Missing |
| DJC / DJR / DJT (global DJ filter) | none | Missing |

## Send FX, mixer, master

| M8 | Nano | Status | Proof |
| --- | --- | --- | --- |
| Mod FX (chorus / phaser / flanger types), rate, depth, width, reverb send | chorus: speed, depth | Partial | `SendFX.cpp`, `fx-screen.rgsim` |
| Delay: time L/R, feedback, width, reverb send | echo: tempo-synced time, feedback | Partial (mono time, no width/ping-pong, no send to reverb) |  |
| Reverb: size, decay, mod, width | size, damp | Partial |  |
| Track faders, FX returns, master | 8 tracks, C/D/R returns, master | Have | `mixer-levels.rgsim` |
| Input strip (line in / USB in level, FX sends) | none | Missing (needs input hardware) |  |
| DJ filter on the master | none | Missing |  |
| OTT / multiband squash | none | Missing |  |
| Master EQ | 3-band with curve | Have | `eq-screen.rgsim` |
| Limiter with look-ahead and GR scope | yes | Have | `master-limiter.rgsim` |
| Master soft clip | Project `Clip`/`Drive` | Have (Nano extra) |  |

## Sample editor processes

| M8 | Nano | Status |
| --- | --- | --- |
| Crop | `crop to S..E` | Have |
| Delete / Duplicate selection | none | Missing |
| Normalize | `normalize` | Have |
| Reverse | `reverse S..E` | Have |
| Invert polarity | none | Missing |
| Fade in / out | both | Have |
| XFade loop | none | Missing |
| Squish (OTT) | none | Missing |
| Mono mix / left / right | none | Missing |
| Downsample, 16-bit, 8-bit | none (crush is a playback knob) | Missing |
| Slice: auto (transients), silence, N equal, lazy chop by tapping | equal slices at playback only | Partial |
| Snap selection to the song's beats | none | Missing |
| Trim silence | `trim silence` | Have (Nano extra) |
| Root note detection | Select on `root` | Have (Nano extra) |

## Navigation: Nano vs M8

Better on the Nano:
- **More buttons.** A, B, LB, RB, Start and Select each have one meaning everywhere, where the M8 overloads Shift/Option/Edit with taps, double-taps and triple-taps (deep clone = "SHIFT+OPTION then double-tap EDIT"; bookmark = "triple-tap OPTION"). The Nano's rule "a combo never fires two actions at once" (Controls.md) is easier to learn.
- **Helper on every screen** (RB+Select: map, keys for this screen, how-to), **built-in guide** with an index, and **undo/redo** everywhere. The M8 has none of these on the device.
- **Pictures on every sound page** (envelopes, filter curve, FM routing, hyper lanes, EQ curve, limiter GR scope) and a tiny piano roll per chain row.
- The **helper map** makes the RB+direction screen map discoverable; the M8's is learned from the manual.

Worse on the Nano, or weak spots:
- **X and Y are copy and paste** (done 2026-09-27): **X** copy, **Y** paste, **LB + Y** new copies, on Song, Chain, Phrase, Table, Groove and Instrument. The user chose this over keyjazz or the command picker: copy, paste and "duplicate as a new phrase/chain" are their most frequent moves.
- **Interpolate exists but isn't in Controls.md or the helper**, so nobody finds it. It is B+RB inside a selection, the same combo as mute outside one. (Clone and deep clone are now X/Y, documented in Controls.md "Copy and paste" and the helper.)
- **Phrase density:** 240 px wide fits note, instrument and 2 FX; the M8's third FX column and velocity column don't fit at this font. A velocity column (2 hex digits) would fit if the instrument name moves to the title (it already shows there).
- **No per-parameter commands** (the M8 lets any knob be sequenced: FM1, SCN, SWM, sends, mixer). On the Nano, automating a knob means a MOD slot or a table.
- **The instrument pool is a dialog, not a screen**, and the Instrument screen is only reachable through a phrase (5 presses from a new song). The Sound Rack fixes this (in progress).
- **Deep menus on 240x240:** 7 instrument pages, reached with LB+Left/Right one at a time (the M8 fits more on one screen). A page list in the helper or X/Y page jumps would help.

## To build next (musical value per effort)

In progress now: **Sound Rack** (instrument pool, keyjazz audition, kit files, new-song template, "compose with it") and **new synth engines + tutorials**. Not repeated below.

1. **Fix `SLCE`** (documented, in the picker, does nothing) and add **LEN**. Small; slicing drum loops is a core M8 move.
2. ~~**Map X and Y.**~~ Done: copy / paste / paste new copies.
3. **Document interpolate** in Controls.md and the helper and give it its own combo (clone and deep clone are done via X/Y). Tiny.
4. **Per-parameter instrument commands**: sends (`SMX/SDL/SRV`), engine knobs (Wav `SIZ/MUL/WRP/SCN`, Hyper `SWM/WID/SUB`, FM `ALG` + op levels, Macro `OSC`), and filter type. Medium; one shared "command → variable" table covers most of them.
5. **Velocity column** (and velocity as a mod-tracking source). Medium; affects phrase layout and every instrument's note-on.
6. **Sample editor processes**: slice auto (transients) + silence + lazy chop into file markers, xfade loop, mono, downsample/8-bit, delete, duplicate, invert, snap to beat. Medium; mostly offline DSP on the existing editor.
7. **Send FX depth**: stereo/ping-pong delay with width, delay→reverb send, reverb mod/width, chorus/phaser/flanger types. Medium; CPU is the constraint, cost each in the DSP harness.
8. **Global mixer commands + DJ filter** (`VMV/VT1–8/VCH/VDE/VRE`, `DJC/DJR/DJT`, `GGR`). Small to medium; great for live mode.
9. **Sequencer extras**: `REP/RTO`, `MTT` micro timing, `ARC`, `INS/NXT`, `OFF` separate from `KILL`, `TBX`, TIC map modes. Small each.
10. **Wavetables** for the Wav engine and FM `W` shapes (scan with a knob/LFO). Medium; licensing of table data must be checked (Plaits/Braids tables are MIT).

Later: song-selection render with range/name, snapshots, per-track scale, themes, OTT, 256 tables, move selection, track time. MIDI and live input depend on firmware/hardware.

## Sources

- [Dirtywave M8 Operation Manual 6.0.0 (2025-06-21)](https://images.equipboard.com/uploads/item/manual/136211/dirtywave-m8-tracker-model-02-manual.pdf)
- [M8Guide: common EFX, sampler, FM, hypersynth/wavsynth, macrosynth references (firmware 6.6)](https://github.com/cengebretson/M8Guide)
- [DirtyWave M8 tips](https://github.com/pauley-unsaturated/DirtyWave-M8-Tips)
