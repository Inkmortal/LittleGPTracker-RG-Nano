# RG Nano / M8 Screen And Capability Audit

Last updated: 2026-09-21

This is the durable build note for M8-style parity work. It tracks what a producer can do today on the RG Nano fork, what the Dirtywave M8 exposes as a reference workflow, what exists only partially, and what should be built next.

## Source Of Truth

- Dirtywave M8 manual v6.5.2, 2026-04-21: https://cdn.shopify.com/s/files/1/0455/0485/6229/files/m8_operation_manual_v20260421.pdf?v=1776791699
- Dirtywave resources page: https://dirtywave.com/pages/resources-downloads
- Dirtywave product/spec overview: https://dirtywave.com/
- Sound On Sound M8 Model:02 workflow review, useful for confirming the practical song/chain/phrase model: https://www.soundonsound.com/reviews/dirtywave-m8-model02
- Anbernic RG Nano technical specification, confirming the 1.54-inch 240x240 screen: https://win.anbernic.com/product/339.html
- M8 manual crop extractor: `tools/extract_m8_manual_screens.py`
- Interactive parity site: `docs/m8-rgnano-parity/index.html`
- RG Nano producer acceptance note: `docs/RGNANO_PRODUCER_ACCEPTANCE.md`
- RG Nano app-only audit script: `projects/resources/RGNANO_SIM/producer-screen-audit-appshots.rgsim`

The M8 visual references in the parity site can be regenerated from exact manual crop rectangles with:

```powershell
python tools\extract_m8_manual_screens.py --scale 4
```

The generated crops are intentionally ignored by git. They are manual-derived assets and should be regenerated locally instead of published.

## Status Legend

| Status | Meaning |
| --- | --- |
| Implemented | Producer workflow exists and has a simulator capture or direct code path. |
| Partial | Some useful equivalent exists, but parity, polish, or tests are incomplete. |
| Missing | No real RG Nano workflow exists yet. Do not imply it exists in UI/docs. |
| Audit needed | Code hints exist, but the producer path or hardware reality is not verified. |
| Out of scope now | Useful M8 feature, but intentionally deferred for the current RG Nano goal. |

## Current RG Nano App Surface

The RG Nano hardware constraint that drives most UI decisions is the 240x240 square display. The parity site should therefore document which RG screens show all columns at once and which screens are compact cursor windows over a wider or longer data model.

The actual RG Nano/LGPT view enum currently exposes:

| RG view type | Source | Producer role |
| --- | --- | --- |
| `VT_SONG` | `sources/Application/Views/SongView.cpp` | Arrange chains across 8 channels. |
| `VT_CHAIN` | `sources/Application/Views/ChainView.cpp` | Sequence phrases and transpose per chain row. |
| `VT_PHRASE` | `sources/Application/Views/PhraseView.cpp` | Main note, instrument, command, and value editor. |
| `VT_PROJECT` | `sources/Application/Views/ProjectView.cpp` | Save/load, tempo, key/scale, note spelling, render mode. |
| `VT_INSTRUMENT` | `sources/Application/Views/InstrumentView.cpp` | Sample/MIDI instrument parameters and sample import entry. |
| `VT_TABLE` | `sources/Application/Views/TableView.cpp` | Phrase-side command automation table. |
| `VT_TABLE2` | `sources/Application/Views/TableView.cpp` | Instrument-side command automation table. |
| `VT_GROOVE` | `sources/Application/Views/GrooveView.cpp` | Groove/tick timing editor. |
| `VT_MIXER` | `sources/Application/Views/MixerView.cpp` | Levels, scopes, waveform monitoring. |

## Documentation/UI Requirements

The parity site should behave more like a compact manual than a marketing page:

- Every M8 map node should cite the manual page and explain the producer job of that screen.
- Every RG Nano map node should explain what exists, how to reach it, what fields are visible, and what to do when the full data model cannot fit on 240x240.
- Producer flow steps should let the reader switch between M8 instructions and RG Nano instructions for the same step.
- RG Nano screens with compact/windowed editing should explicitly say whether all columns fit, whether vertical scrolling is used, or whether another screen/table/modal holds the extra controls.
- Missing RG Nano equivalents must be labeled as missing, not illustrated as though they exist.

Modal and overlay workflows also matter:

| Modal/overlay | Source | Producer role |
| --- | --- | --- |
| Project selector/new project | `SelectProjectDialog`, `NewProjectDialog` | Start or create a project. |
| Import sample dialog | `ImportSampleDialog` | Preview/import WAVs into sample library/instruments. |
| Record sample dialog | `RecordSampleDialog` | Live recording path exists in code, but RG Nano hardware input is not current scope. |
| Power menu/debug overlay | `SDLEventManager`/SDL UI | App exit/debug path. Real RG Nano Menu/Power maps to `q`; simulator maps to `p`. |

## Structural Capability Audit

| Capability | M8 reference | RG Nano status | Evidence | Build note |
| --- | --- | --- | --- | --- |
| Tracks/channels | 8 monophonic tracks | Implemented | `SONG_CHANNEL_COUNT 8`; all-8-channel sim coverage noted in acceptance doc | Keep testing all 8 channels after layout/audio changes. |
| Song length | Song arranger rows | Implemented | `SONG_ROW_COUNT 256` | Document max row count for users. |
| Chains | Chain list per track | Implemented | `CHAIN_COUNT 0xFF`; `ChainView` | Good core parity. |
| Phrases | 16-step phrase units | Implemented | `PHRASE_COUNT 0xFF`; `PhraseView`; phrase data is 16 rows | Good core parity, but phrase screen is highest polish priority. |
| Instruments | 128 M8 instruments | Partial | RG has `MAX_SAMPLEINSTRUMENT_COUNT 0x80` plus `MAX_MIDIINSTRUMENT_COUNT 0x10` | RG has enough slots, but lacks M8 synth engines and pool UX. |
| Tables | 256 M8 instrument tables | Partial | RG has `TABLE_COUNT 0x80`, phrase/instrument table views | Strong existing movement tool; count is lower than M8. |
| Grooves | Groove/timing table | Implemented | `MAX_GROOVES 0x20`; `GrooveView` | Good, needs producer docs. |
| Scales/key | Dedicated M8 Scale View | Partial | Project `Key`, `Scale`, `Notes`; scale-aware phrase edits | Needs dedicated compact Scale screen or improved project route. |
| Sample import | M8 sampler/SD workflow | Implemented for uploaded WAV | `SamplePool`, `ImportSampleDialog`, sample import sim capture | Core RG workaround for no mic. Needs better sample management. |
| Live sampling | M8 Model:02 mic/line sampling | Out of scope now | RG Nano no mic; `RecordSampleDialog` not hardware-proven | Do not prioritize until hardware path is known. |
| Render/bounce | M8 Render View, selection-to-sample | Partial | `ProjectView` render mode; native Off/Stereo/Stems; `render-export-workflow.rgsim` verifies `mixdown.wav` and `channel0.wav` | Stereo/stems bounce works. Missing M8-style render range/name controls and render-to-sample. |
| Mixer/meters | M8 Mixer, EQ, limiter/scope | Partial | `MixerView`, `MixerService`, phrase/mixer waveforms | Mixer exists; EQ/limiter parity missing. |
| MIDI | M8 MIDI Mapping/Settings/MIDI Out instrument | Audit needed | MIDI services and MIDI instruments exist | Needs RG Nano hardware/USB route proof. |
| Theme | M8 Theme View | Missing | App has config colors, no producer theme screen | Low priority. One strong readable theme is enough for now. |
| System/time stats | M8 System and Time Stats | Partial | Config, logs, simulator debug | Not a producer priority, but debug overlay can carry this. |

## Screen-By-Screen Parity Audit

| M8 screen | Manual source | Website visual source | RG Nano equivalent | Status | Next build/audit action |
| --- | --- | --- | --- | --- | --- |
| Song View | Manual p. 10 | Exact manual crop, dark rendered | Song View | Implemented | Keep all 8 channels visible; document 256 rows. |
| Live Mode | Manual p. 10 | Same page crop, dark rendered | Song/Live behavior via playback/cue concepts | Partial | Verify real RG live/cue controls and capture a dedicated live-mode screenshot. |
| Chain View | Manual p. 12 | Exact manual crop, dark rendered | Chain View | Implemented | Add route/navigation doc and test transpose editing. |
| Phrase View | Manual p. 14 | Exact manual crop, dark rendered | Phrase View | Implemented | Keep waveform smooth and nonintrusive; add command edit coverage. |
| Instrument View | Manual p. 16 | Exact manual crop, dark rendered | Instrument View | Partial | Audit all fields on 240x240; identify cramped labels; plan engines. |
| Instrument Modulation View | Manual p. 18 | Exact manual crop, dark rendered | Tables/instrument table only | Missing | Design compact modulation page or table-driven equivalent. |
| Instrument Pool View | Manual p. 22 | Exact manual crop, dark rendered | No dedicated pool | Missing | Build compact instrument pool/browser for copy/manage/audition. |
| Table View | Manual p. 24 | Exact manual crop, dark rendered | Table View and Table2 | Implemented | Add tests for both phrase table and instrument table command selection. |
| Groove View | Manual p. 26 | Exact manual crop, dark rendered | Groove View | Implemented | Add docs explaining groove use in song production. |
| Scale View | Manual p. 28 | Exact manual crop, dark rendered | Project Key/Scale fields | Partial | Decide between dedicated Scale screen vs Project subpage. |
| Mixer View | Manual p. 30 | Exact manual crop, dark rendered | Mixer View | Partial | Keep channel scopes; add clearer level controls if missing. |
| EQ Editor View | Manual p. 32 | Exact manual crop, dark rendered | None dedicated | Missing | Research current filter/EQ capabilities, then design minimal EQ screen. |
| Limiter & Mix Scope View | Manual p. 34 | Extractor creates crop, site currently groups under EQ/Limiter | Mixer waveform only | Partial | Add separate parity node or detail entry; audit limiter/render clipping behavior. |
| Effect Settings View | Manual p. 36 | Exact manual crop, dark rendered | Project `Reverb`/`Damp`/`Echo`/`Fdbk` + per-synth reverb/delay sends | Partial | Shared reverb and tempo-synced echo exist (`Mixer/SendFX`). No chorus/mod FX, sample instruments do not send yet. |
| Project View | Manual p. 38 | Exact manual crop, dark rendered | Project View | Implemented | Improve render/save/key docs; verify field order after changes. |
| System Settings View | Manual p. 40 | Exact manual crop, dark rendered | Config/debug paths | Partial | Keep low priority unless hardware setting blocks production. |
| Theme View | Manual p. 42 | Exact manual crop, dark rendered | Config colors only | Missing | Low priority; do not build before core music features. |
| MIDI Mapping View | Manual p. 43 | Extractor creates crop, site currently groups under MIDI | MIDI code exists | Audit needed | Verify physical/USB MIDI on RG Nano and document constraints. |
| MIDI Settings View | Manual p. 44 | Exact manual crop, dark rendered | MIDI config/code exists | Audit needed | Same as above. |
| Time Stats View | Manual p. 46 | Not currently mapped in site | Debug/log equivalent | Missing/low priority | Consider debug overlay, not a main producer screen. |
| Render View | Manual p. 47 | Exact manual crop, dark rendered | Project render mode | Partial | Native Project bounce is tested for Stereo/Stems. Add clearer range/name/output feedback and render-to-sample parity. |
| Selection to Sample | Manual p. 48 | Extractor captures page, not currently a site node | None | Missing | High value for no-live-sampling workflow: render selected song/chain to sample. |
| Effect Command Help View | Manual p. 48 | Exact manual crop, dark rendered | Command selector exists in command columns | Partial | Confirm universal shortcut and add contextual command help test. |
| Wavsynth | Manual p. 50 | Not currently a site node | Synth instrument (`SynthInstrument`) | Implemented | Sine/triangle/saw/pulse/supersaw/noise/metal with shape knob, sub, ADSR, filter, LFO, 17 presets, CHRD chords. |
| Macrosynth | Manual p. 52 | Not currently a site node | None | Missing | Later engine; study open-source references. |
| Sampler | Manual p. 54 | Exact manual crop, dark rendered | Sample instrument/import | Partial | Add deeper sample parameter/tutorial coverage. |
| Sample Editor | Manual p. 56 | Extractor creates crop, site currently groups under Sampler | None dedicated | Missing | Build crop/loop/slice/normalize editor after render/export basics. |
| FM Synth | Manual p. 58 | Not currently a site node | Synth `fm`/`ratio` knobs | Partial | 2-operator phase modulation with envelope-driven brightness (KEYS, BELL presets). No 4-op algorithms. |
| Hypersynth | Manual p. 60 | Not currently a site node | Synth `supersaw` wave + `chord`/CHRD | Partial | Detuned supersaw with one-note chords; no per-voice chord memory like M8. |
| External Instrument | Manual p. 62 | Not currently a site node | Maybe MIDI/external audio only | Out of scope/audit needed | Hardware path likely limits this. |
| MIDI Out Instrument | Manual p. 64 | Not currently a site node | MIDI instrument code exists | Audit needed | Verify RG Nano MIDI output before UX promises. |

## Producer Workflow Audit

| Producer job | Can RG Nano do it today? | Confidence | Notes |
| --- | --- | --- | --- |
| Start/load project | Yes | High | Project selector and new project dialog captured. |
| Create a loop | Yes | High | Song -> Chain -> Phrase flow exists. |
| Use all 8 channels | Yes | High | Code constant and prior sim workflow coverage. |
| Enter notes fast | Yes | Medium-high | Phrase editor works; scale/chromatic override exists. Needs shortcut docs. |
| Enter notes outside scale | Yes | Medium-high | `L + D-pad` chromatic override is documented in acceptance note. |
| Change key/scale mid-writing | Project-level yes, time-varying no | Medium | Existing notes are not rewritten. No row-based key command yet. |
| Use sharps/flats | Yes | Medium-high | Project `Notes:` setting controls display spelling. |
| Load uploaded WAV samples | Yes | High | Import sample dialog and SamplePool path exist. |
| Preview samples before import | Yes | Medium-high | Acceptance doc notes native Listen preview is assertable. |
| Edit sample start/loop/slice deeply | Limited | Low-medium | Instrument params exist, but no M8-like sample editor. |
| Record live audio | Not target now | Low | Code exists, hardware path not trusted. |
| Make movement/automation | Yes, through tables | Medium | Powerful but needs discoverability and command tests. |
| Use native synth engines | Yes | High | New projects start with a 16-instrument synth kit; `type` switches any slot between synth and sample. |
| Mix levels with visual feedback | Partially | Medium | Mixer scopes/waveforms exist; actual level editing parity needs audit. |
| EQ/limiter/mastering | Mostly no | Medium | M8 has dedicated screens; RG has effects/filter pieces but no parity UI. |
| Bounce a song | Yes, basic stereo/stems | Medium-low | Project `Render: Stereo` writes `mixdown.wav`; `Render: Stems` writes `channelN.wav` files. Needs M8-style range/name/render-to-sample controls. |
| Resample selection into instrument | No | High | Important future feature because RG lacks live sampling. |
| Use external MIDI gear | Unknown | Low | Code exists, hardware path unverified. |
| Learn commands in context | Yes for Phrase/Table command columns, plus a universal helper | Medium-high | Raw `Select` opens the command selector/help picker while focused on Phrase or Table command columns. `R + Select` opens a paged helper: minimap first, then `Up`/`Down` shows current-screen commands. `command-runtime-workflow.rgsim` verifies measurable `TABL`, `GROV`, and `TMPO` behavior. |

## Immediate Build Backlog

### P0: Stop Guessing

1. Keep `tools/extract_m8_manual_screens.py` as the only source for M8 visual reference crops.
2. Add an audit flag in the parity site for representative nodes, especially EQ/Limiter, MIDI, Sampler/Sample Editor, and Selection to Sample.
3. Regenerate RG Nano app-only screenshots after every layout change.

### P1: Producer Core

1. Command selector/help audit and test:
   - Phrase command column.
   - Phrase table command column.
   - Instrument table command column.
   - Extend command runtime coverage to audio-shaping commands: volume automation, retrigger, pitch/legato, and sample offset.
2. Render/export workflow:
   - Verify `Off`, `Stereo`, and `Stems`.
   - Verify output file path and completion notification.
   - Add simulator assertions and documentation.
3. Sample import/editor workflow:
   - Keep upload/import preview.
   - Add clear sample assignment confirmation.
   - Plan start/end/loop/slice/normalize operations.

### P2: M8 Power Features In Tiny Form

1. Instrument Pool/browser.
2. Dedicated compact Scale screen or Project subpage.
3. Minimal mixer controls and limiter/clipping feedback.
4. First native synth engine, likely a simple wavetable/wavsynth before FM/macrosynth.

### P3: Studio/External Workflow

1. MIDI hardware audit.
2. MIDI mapping/settings UX if hardware path works.
3. External instrument/audio monitoring only if the RG Nano path is real.

## Site Accuracy Notes

- M8 visuals: accurate to the official manual crop geometry when generated locally, then stylized to a dark display. Not literal live M8 firmware screenshots.
- RG Nano visuals: accurate to simulator captures for audited screens.
- M8 capabilities: drawn from the official manual and product overview.
- RG Nano capabilities: drawn from current code, simulator scripts, and acceptance docs.
- Any missing RG feature must be labeled missing. Do not show speculative screens as if they exist.
