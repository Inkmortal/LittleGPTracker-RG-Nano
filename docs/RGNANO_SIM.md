# RG Nano Simulator

`RGNANO_SIM` is a desktop SDL build that keeps the RG Nano app contract:

- 240x240 logical screen
- 240x240 SDL surface by default, matching the RG Nano pixel dimensions
- RG Nano button names and compact key layout
- desktop audio output through the Windows SDL/W32 backend
- event logging through `DUMPEVENT`
- normal LGPT project/sample folders under `rgnano-sim-data`

## Build

```powershell
.\run.ps1 -Task dev
```

The current workspace uses MSYS2 MinGW and the repo's bundled SDL1 library for this target.

## Run

Copy `projects/resources/RGNANO_SIM/config.xml` and `mapping.xml` next to the simulator executable, then run:

```powershell
.\lgpt-rgnano-sim.exe
```

Or use the helper:

```powershell
.\tools\run-rgnano-sim.ps1
```

Running without a script opens the RG Nano skin by default. The skin has a 240x240 tracker screen, a colored RG Nano-inspired body, clickable simulated controls, top shoulder buttons, USB-C detail, and side power/SD-card hints. Scripted smoke tests keep the exact bare 240x240 SDL surface unless you pass `-Skin`.

```powershell
.\tools\run-rgnano-sim.ps1 -Skin
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\smoke.rgsim
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\smoke.rgsim -Skin
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\sample-fixture.rgsim -Skin -SeedSampleFixture -ArtifactsDir .\sim-artifacts
```

Use `-SeedLofiFixture` to generate a built-in sample pack for producer workflow tests. It currently creates drum, hat, chord, vinyl-bed, guzheng-like, dizi-like, erhu-like, and traditional drum WAVs locally so simulator tests stay reproducible and license-clean:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\lofi-sample-pack.rgsim -SeedLofiFixture -ArtifactsDir .\sim-artifacts
```

The Wuxia/donghua lofi studio workflow imports that palette, programs an 8-channel tracker arrangement, plays it through the simulator, and captures a two-minute WAV you can listen to:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\wuxia-lofi-studio.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts-wuxia-lofi
```

The all-channel workflow is the compact RG Nano playback regression. It programs all 8 tracker channels, verifies each channel is producing its assigned instrument, captures audio, and screenshots the live bottom note/instrument monitor plus the one-row audio meter strip:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\all-8-channels-workflow.rgsim -ResetLastProject -SeedLofiFixture -ArtifactsDir .\sim-artifacts-all-8
```

The mixer waveform workflow programs a minimal song, starts playback, opens Mixer, asserts that the live master waveform is visible, captures audio, and screenshots the 240x240 Mixer layout:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\mixer-waveform-workflow.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts-mixer-waveform
```

The universal context overlay workflow verifies that `R + Select` opens the same helper layer from real tracker views, that `Up`/`Down` pages between minimap and context commands, and that it closes without leaking inputs into the song:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\context-overlay-workflow.rgsim -ResetLastProject -Skin -ArtifactsDir .\sim-artifacts-context-overlay
```

Direct executable form, useful when the helper script is not available:

```powershell
cd .\projects
.\lgpt-rgnano-sim.exe -AUTO_LOAD_LAST=NO -RGNANOSIM_SCRIPT=resources\RGNANO_SIM\context-overlay-workflow.rgsim
```

For release confidence, use the all-screens helper coverage script. It creates a project, walks the main producer views, opens the `R + Select` minimap, flips to the command page, verifies screen-specific helper text, then closes the helper without changing views:

```powershell
.\lgpt-rgnano-sim.exe -AUTO_LOAD_LAST=NO -RGNANOSIM_SCRIPT=resources\RGNANO_SIM\context-overlay-all-screens.rgsim
```

Modal helper coverage verifies that `R + Select` is also available before a song is loaded and inside startup dialogs:

```powershell
.\lgpt-rgnano-sim.exe -AUTO_LOAD_LAST=NO -RGNANOSIM_SCRIPT=resources\RGNANO_SIM\context-overlay-modal-screens.rgsim
.\lgpt-rgnano-sim.exe -AUTO_LOAD_LAST=NO -RGNANOSIM_SCRIPT=resources\RGNANO_SIM\context-overlay-sample-import-modal.rgsim
.\lgpt-rgnano-sim.exe -AUTO_LOAD_LAST=NO -RGNANOSIM_SCRIPT=resources\RGNANO_SIM\context-overlay-command-modal.rgsim
.\lgpt-rgnano-sim.exe -AUTO_LOAD_LAST=NO -RGNANOSIM_SCRIPT=resources\RGNANO_SIM\context-overlay-global-menu-screens.rgsim
```

The command selector workflow verifies a core tracker producer move: create a song/chain/phrase, open the Phrase command selector with `Select`, confirm `ARPG`, edit its parameter, then open the same selector from Table view:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\command-selector-workflow.rgsim -ResetLastProject -Skin -ArtifactsDir .\sim-artifacts-command-selector
```

The command runtime workflow verifies measurable command behavior while the song is playing: `TABL` starts table playback, `GROV` changes channel groove state, and `TMPO` changes the project tempo:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\command-runtime-workflow.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts-command-runtime
```

The playback scope workflow verifies that the visible `PLAY:` label matches actual tracker mode from Song, Chain, Phrase, and Table, including Phrase `R + Start` for song-context playback:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\playback-scope-workflow.rgsim -ResetLastProject -Skin -ArtifactsDir .\sim-artifacts-playback-scope
```

The producer persistence pair creates a multi-instrument project, saves it, relaunches through `AUTO_LOAD_LAST`, verifies tempo/song/chain/phrase/sample bindings, and captures audio from the reopened project:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\producer-persistence-create.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts-producer-persist-create
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\producer-persistence-reopen.rgsim -Skin -ArtifactsDir .\sim-artifacts-producer-persist-reopen
```

## Headless, Silent Runs

Scripted runs never open a window and never play through your speakers:

- `run-rgnano-sim.ps1 -Script ...` passes `-RGNANOSIM_HEADLESS=YES`: the simulator hands SDL a window that is never shown, so nothing appears on screen or takes focus. Screenshots, screen-text assertions and audio captures all still work. Pass `-Visible` to watch a script run in a window. (`SDL_VIDEODRIVER=dummy` from older tools also switches on headless mode.)
- `-Mute` (the suite passes it by default) sends silence to the sound card while still measuring and capturing the real audio. Use `run-rgnano-sim-suite.ps1 -Audible` to hear it.
- The simulator is linked as a Windows GUI app and the suite runs every case inside one PowerShell process, so no console windows pop up either.

The simulator surface is exactly 240x240, the RG Nano's 1.54-inch panel resolution; `expect_size 240 240` asserts it.

## Goal Commands (say what you want, not which keys)

Instead of counting button presses, scripts can state a goal. The simulator reads the live screen after every input and keeps pressing the right combo until the goal is met, failing with a clear log line if an input changes nothing:

```text
goto instrument          # shortest RB+Dpad route from wherever you are
page FILTER              # LB+Right until the instrument page matches
focus cutoff             # move the field cursor to the row labelled "cutoff"
set cutoff 0x80          # focus + A+Dpad until the value is 0x80
set filter lowpass       # list fields step toward the named entry
set preset bell          # load a synth preset
set type sample          # switch the slot to a sample instrument
instrument 0A            # B+Left/Right/Up/Down to instrument 0A
row 0C                   # move the Song/Chain/Phrase cursor to row 0C
```

`goto` knows the view graph (Song, Chain, Phrase, Instrument, Table, Groove, Project, Mixer). `focus`/`set` work on any field screen (Instrument, Project). Each command logs `=> reached in N steps`.

## Synth Script Commands

```text
sim_set_synth 3 pluck                 # make slot 03 a synth with a preset
sim_set_instrument_param 3 cutoff 0x60  # any instrument param by name ('_' for spaces)
expect_instrument_type 0 Synth        # Synth / Sample / Midi
expect_instrument_name 0 KICK
expect_instrument_param 0 env_amount 0x30
```

## Music Tools

- `python tools/lgpt_composer.py` builds every song in `tools/demos/*.py` into `projects/resources/demos/lgpt_<Name>/lgptsav.dat` (real project files).
- `python tools/render_demos.py [--only name] [--stems]` builds, loads each song in the simulator, bounces it with the app's own Stereo render, and writes WAV + JSON + spectrogram PNG to `sim-artifacts-demos`. `--stems` also prints per-track levels for mixing.
- `python tools/audio_report.py file.wav --png out.png` prints peak/RMS/clipping/band energy/key histogram for any WAV.
- If the simulator crashes, it writes `rgnano-sim-crash.txt`; `python tools/symbolize_crash.py` turns it into function names and source lines.

## Buttons

| RG Nano | Simulator key |
| --- | --- |
| Up | `u` |
| Down | `d` |
| Left | `l` |
| Right | `r` |
| A | `a` |
| B | `b` |
| X | `x` |
| Y | `y` |
| L | `m` |
| R | `n` |
| Start | `s` |
| Select / FN | `k` |
| Power menu | `p` |

In skin mode, these controls can also be clicked directly on the device shell.

This target is intended for automated UI, project, sample, and audio-output smoke tests before moving a build to the physical RG Nano.

The controls are documented in the guide ([Controls](rgnano-wiki/Controls.md)) and guarded by the `key-grammar` sim test. Use them as the authority for simulator workflows: LGPT handles held button masks, so tests should use real combos instead of treating buttons as isolated clicks.

## Scripted Smoke Tests

Pass a script with:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\smoke.rgsim
```

Script commands:

```text
# milliseconds
wait 500

# key duration-ms
press a 80
press n 80
press r 80

# hold/release
down m
press k 80
up m

# click x y duration-ms
click 250 372 100

# SDL BMP screenshot
screenshot smoke-song.bmp

# fail unless a file exists
expect_file rgnano-sim-data/samples/rgnano-test-tone.wav

# fail unless a sample was imported into the active project's samples folder
expect_project_sample rgnano-test-tone.wav

# a pool sample's frame count ("-" to skip) and peak level within min..max
expect_sample_stats lofi-chord_nrm.wav 32000 32700 32767

# apply a saved PARAM the way a song load does (old names like "ping pong"
# are translated); the value is the rest of the line
sim_load_instrument_param 0 loopmode ping pong

# fail unless the simulator log contains text
expect_log Loaded

# fail unless the active LGPT view matches the source-derived view name
expect_view song

# fail unless visible text is present in the current 40x30 character screen
expect_screen_text 1######

# fail unless the currently inverted/highlighted screen text contains a phrase
expect_selected_text Import

# fail unless LGPT's native player/preview state matches the intended workflow
expect_player_running yes
expect_player_running no
expect_play_mode song
expect_play_mode phrase
expect_streaming_sample rgnano-test-tone.wav
expect_playing_channel 0 00
expect_skin_frame_clean
expect_screens_differ before.bmp after.bmp

# write current view, cursor/model state, highlighted text, and full 40x30 text screen to the log
dump_state sample import open

# fail unless tracker model data matches what the script programmed
expect_song_chain 0 0 00
expect_chain_phrase 0 0 00
expect_phrase_row_count 0 8
expect_phrase_command 0 0 1 ARPG
expect_phrase_param 0 0 1 0001
expect_groove 0 1
expect_table_active 0 0
expect_tempo 86
expect_instrument_sample 4 wuxia-guzheng.wav

# directly program tracker state for long producer workflows
sim_set_tempo 82
sim_set_key 2
sim_set_scale 21
sim_set_note_names flats
sim_import_sample_to_instrument 3 wuxia-guzheng.wav
sim_set_song_chain 0 3 3
sim_set_chain_phrase 3 0 3 0
sim_set_phrase_note 3 0 50 3
sim_set_phrase_command 3 0 1 TABL 0000
sim_set_table_command 0 0 1 VOLM 0040
sim_save_project

# reset and assert measured non-silent tracker audio output
reset_audio_stats
start_audio_capture demo-song-workflow.wav
expect_audio_activity 64
expect_audio_capture_bytes 120000
end_audio_capture

# fail unless no measured tracker audio has been produced since reset_audio_stats
expect_audio_silence 0

# fail if the simulator log contains an error marker
expect_no_error

# fail unless the SDL surface is exactly 240x240
expect_size 240 240

# fail unless the screen has at least two distinct pixel values
expect_colors 2

quit
```

Screenshots and assertions are captured from the SDL surface, so they are available to automated tests without a camera or physical RG Nano.

Every scripted command now logs a concise `RGNANO_SIM_STATE` line with the active view, tracker cursor/model coordinates, and highlighted text. Use `dump_state <label>` when a flow needs a full `RGNANO_SIM_SCREEN` dump of the 40x30 character screen, including selection/highlight segments. On script failure the simulator writes the full dump automatically.

Scripts can also use source-derived route helpers. The C++ simulator expands these routes into ordinary button events before executing the script:

```text
route boot.new_project_random
route project.to_song
route song.to_mixer
route mixer.to_song
route song.to_chain
route chain.to_phrase
route phrase.to_instrument
route instrument.open_sample_import
route sample_import.to_first_file
route sample_import.import_selected
```

Route helpers are built into the simulator and mirrored in `tools\rgnano-sim-routes.ps1` for older runner flows. They are intentionally small and explicit, so routes like `phrase.to_table` remain tied to the source map instead of hidden in test-specific scripts.

The canonical smoke test is:

```powershell
.\run.ps1 -Task smoke
```

The full producer/device regression suite builds the simulator, runs every canonical workflow in sequence, stores each run's logs/screenshots/WAV captures under `sim-artifacts-suite`, and writes `suite-summary.json`:

```powershell
.\run.ps1 -Task suite
```

The sample fixture smoke test generates a tiny 440 Hz WAV file in `rgnano-sim-data/samples`, verifies it exists, checks the simulator log, exercises a skinned button click, captures a screenshot, and copies logs/screenshots into `sim-artifacts`:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\sample-fixture.rgsim -Skin -SeedSampleFixture -ArtifactsDir .\sim-artifacts
```

The source-derived new-project route drives the actual boot controls from `SelectProjectDialog` and `NewProjectDialog`:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\new-project-route.rgsim -ResetLastProject -ArtifactsDir .\sim-artifacts
```

The first source-derived music workflow creates a new project, creates a chain, creates a phrase, creates the first note/instrument, opens Instrument, then opens the sample import modal:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\basic-music-workflow.rgsim -ResetLastProject -ArtifactsDir .\sim-artifacts
```

The uploaded-sample workflow seeds the simulator sample library, uses the native `Listen` action to preview the selected WAV, imports it into the active project, and asserts the project sample exists:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\sample-import-workflow.rgsim -ResetLastProject -SeedSampleFixture -ArtifactsDir .\sim-artifacts
```

The demo-song workflow drives the real RG Nano controls to create a song, chain, and phrase, enters an 8-step melodic pattern, imports the seeded WAV onto instrument `00`, starts native phrase playback, asserts the player mode/channel state, writes a listenable `demo-song-workflow.wav`, asserts measured audio activity, and verifies the Chain view activity meter in the unused sidebar space:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\demo-song-workflow.rgsim -ResetLastProject -SeedSampleFixture -Skin -ArtifactsDir .\sim-artifacts
```

The all-8-channels workflow assigns independent chains, phrases, notes, and instruments to all 8 tracker channels, starts song playback, and asserts that every channel reports active playback:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\all-8-channels-workflow.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts
```

The render/export workflow uses the app's native Project `Render:` field. It sets `Stereo`, starts/stops song playback, and asserts the active project contains a non-empty `mixdown.wav`; then it sets `Stems` and asserts `channel0.wav` exists with audio-sized data:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\render-export-workflow.rgsim -ResetLastProject -SeedLofiFixture -ArtifactsDir .\sim-artifacts
```

The mixer waveform workflow verifies the Mixer screen's live master waveform during song playback:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\mixer-waveform-workflow.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts
```

The mixer waveform minute workflow is a longer visual/listening pass. It builds the Wuxia/lofi arrangement, switches to Mixer, captures about one minute of audio, and saves start/mid/end shell plus exact 240x240 app screenshots of the hybrid waveform:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\mixer-waveform-minute.rgsim -ResetLastProject -SeedLofiFixture -Skin -ArtifactsDir .\sim-artifacts-waveform-minute
```

The note-spelling workflow verifies that the same pitch can be displayed with sharp or flat note names from the Project `Notes:` setting:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\note-spelling-workflow.rgsim -ResetLastProject -ArtifactsDir .\sim-artifacts
```

The scale/key workflow verifies that Project `Key:` is visible, that Project `Scale:` remains visible, and that the existing Phrase `A + D-pad` note edit follows the selected key without adding another cramped shortcut:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\scale-key-workflow.rgsim -ResetLastProject -ArtifactsDir .\sim-artifacts
```

The scale/free override workflow verifies the RG Nano button economy for note entry: new projects default to Project `Key: --`, `A + D-pad` stays chromatic until a key is chosen, and `L + D-pad` on a note cell is the quick chromatic override once key/scale snapping is active:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\scale-free-override-workflow.rgsim -ResetLastProject -ArtifactsDir .\sim-artifacts
```

The producer navigation tour exercises the main RG Nano view routes after creating song/chain/phrase state:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\producer-navigation-tour.rgsim -ResetLastProject -Skin -ArtifactsDir .\sim-artifacts
```

The producer screen audit walks every core producer-facing view, captures a screenshot for each, and writes `dump_state` logs with the active view, cursor, selected text, and player state:

```powershell
.\tools\run-rgnano-sim.ps1 -Script .\projects\resources\RGNANO_SIM\producer-screen-audit.rgsim -ResetLastProject -SeedSampleFixture -Skin -ArtifactsDir .\sim-artifacts-screen-audit
```

For the stricter app-only readability check, use the Python runner. It avoids the PowerShell helper path, captures the 240x240 app screens, checks for clipped edges, missing regions, and dense unreadable rows, then cleans up generated screenshots:

```powershell
python tools\run_producer_layout_audit.py
```
