# Developer Guide

Everything runs on Windows with MSYS2 (MinGW32) for the simulator and WSL (Ubuntu) for the RG Nano cross-build. All tools are in `tools/`.

## Build

| Target | Command | Output |
| --- | --- | --- |
| Desktop simulator | `.\run.ps1 -Task dev` (or `tools\build-rgnano-sim.ps1`) | `projects/lgpt-rgnano-sim.exe` |
| RG Nano | `cd projects; make PLATFORM=RGNANO` inside WSL with the FunKey SDK in `sdk/FunKey-sdk-DrUm78` | `projects/lgpt-rgnano.elf` |
| RG Nano + install | `tools\install-rgnano.ps1` | builds, packages the OPK and copies app, docs and demos to a connected RG Nano |

CI (`.github/workflows/build-rgnano.yml`) builds the ELF, the OPK and a zip with the demo songs and docs.

## Simulator

`lgpt-rgnano-sim.exe` is the real app with a 240×240 SDL surface, the RG Nano key layout and a script runner.

```powershell
.\tools\run-rgnano-sim.ps1                    # interactive, with the device skin
.\tools\run-rgnano-sim.ps1 -Script x.rgsim    # headless: hidden window, no focus stealing
.\tools\run-rgnano-sim.ps1 -Script x.rgsim -Mute -Visible
.\tools\run-rgnano-sim-suite.ps1              # every regression case, muted and headless
```

Scripted runs render into a window that is never shown, so nothing appears on screen. Screenshots, screen-text checks and audio capture still work. `-Mute` sends silence to the sound card while still measuring and capturing the real output.

### Script language

Low level: `press a 80`, `down n` / `up n` (hold), `wait 500`, `screenshot_app x.bmp`.

Goal commands — say what you want; the simulator reads the live state and presses the keys:

```text
goto instrument        # shortest RB+D-pad route
row 0C                 # cursor row on Song/Chain/Phrase
page FILTER            # instrument page
focus cutoff           # field cursor to a label
set cutoff 0x80        # focus + edit until the value matches
set preset bell        # list values by name
instrument 0A          # B+D-pad to an instrument slot
```

Each goal logs `=> reached in N steps` and fails with a clear message if an input changes nothing.

Assertions: `expect_view`, `expect_screen_text`, `expect_selected_text`, `expect_player_running`, `expect_play_mode`, `expect_audio_activity`, `expect_song_chain`, `expect_phrase_row_count`, `expect_instrument_type/name/param`, `expect_audio_peak_max`, `expect_limiter_gr`, `expect_size 240 240`, `expect_no_error`, and more — see `docs/RGNANO_SIM.md`.

State setup for long scenarios: `sim_set_synth`, `sim_set_instrument_param`, `sim_set_song_chain`, `sim_set_chain_phrase`, `sim_set_phrase_note`, `sim_set_phrase_command`, `sim_set_tempo`.

Whole songs: `sim_dump_song file.txt` writes everything a song is made of (project settings, song grid, chains, phrases, every instrument knob, tables, grooves, mixer levels) as text; `expect_song_dump file.txt` fails, listing the differing lines, unless the current song matches it.

`run-rgnano-sim.ps1` options for long scripts: `-SeedSamplePacks` copies the shipped packs (`projects/resources/samples`) into the sim's `Applications/Samples`, `-NameSeed 7` makes the suggested song names the same every run, `-NoKeyRepeat` turns key auto-repeat off so a busy machine can't turn one press into two.

### Your First Song is a test

`tools/walkthrough/steps.py` lists every button press of [Your First Song](Your-First-Song) with its caption. `python tools\make_walkthrough.py` turns it into the suite case `first-song-walkthrough.rgsim`, runs it in the simulator from a fresh start with a screenshot after every step, draws each one above a picture of the RG Nano's buttons (`docs/rgnano-wiki/images/walkthrough/`) and writes the wiki page; then run `python tools\build_ingame_guide.py`. The test ends with `expect_song_dump` against the Afterglow demo (`tools/demos/afterglow.py`, dumped by the `first-song-reference` case just before it), so a change in the app that breaks the walkthrough fails the suite. `--draft` captures without that final check while you edit steps; `--update-asset` refreshes the bar the walkthrough resamples (`tools/demos/assets/afterglow-rs_01.wav`). Change the song in `afterglow.py` and `steps.py` together.

### When the simulator crashes

It writes `rgnano-sim-crash.txt`. `python tools\symbolize_crash.py` turns it into function names and source lines.

## Song composer

`tools/lgpt_composer.py` writes real `lgptsav.dat` projects from Python, with helpers for note names, chord voicing (smooth inversions that fit one `CHRD`), drum pattern strings and phrase text:

```python
from pathlib import Path
from lgpt_composer import Phrase, Project
from _patterns import chord_bar, voice_progression

p = Project("MySong", tempo=100)
p.synth(0, "kick")
p.synth(5, "pad", reverb=0x90)
kick = p.chain([Phrase.drums("x...x...x...x...", 0)] * 4)
pads = p.chain([chord_bar(v, 5) for v in voice_progression(["Am", "F", "C", "G"], "E3")])
p.row(0, [kick, None, None, None, None, pads, None, None])
p.save(Path("rgnano-sim-data/tracks"))
```

Demo songs live in `tools/demos/*.py`.

## Listening without ears

| Tool | Does |
| --- | --- |
| `python tools\render_demos.py [--only name] [--stems]` | builds each demo, bounces it with the app's own Stereo render, writes WAV + JSON + spectrogram PNG to `sim-artifacts-demos`; `--stems` prints per-track levels for mixing |
| `python tools\audio_report.py file.wav --png out.png` | peak, RMS, crest, clipping, silent seconds, band energy, pitch classes |
| `python tools\capture_wiki_screens.py` | regenerates every screenshot on this wiki |

## Control design rules

Every screen follows the key grammar in [Controls](Controls). When adding a combo, check it against these (from game-controller UX practice: Nielsen's heuristics, the Game Accessibility Guidelines, Xbox accessibility guideline 107, Swink's *Game Feel*):

- **Act on press.** Nothing waits for a release; B backs out the instant it is pressed.
- **Feedback on every press**, drawn by the next frame; an action that can't happen says why (`Empty: press A for a chain`) instead of doing nothing.
- **Cost matches frequency.** Move, add a note, play: one button. Copy, jump, big steps: one modifier + one button. Never three buttons for something routine.
- **One meaning per combo per screen**, and the same meaning on every screen. A combo never fires two actions.
- **No timing tricks.** No double-tap windows; live cues wait for the bar, not for a precise press.
- **B is always the way out** of a dialog, without side effects.
- **Everything is undoable.** `UndoHistory` snapshots the song, chains, phrases, tables, grooves, mixer, project settings and the current instrument before every press and keeps it if the press changed something (one A-hold = one step, 32 steps). New editing code needs nothing extra; new data outside those needs adding to `UndoHistory::Capture`.
- **Show the mode** (Song/Live, selection, playing) on screen; the helper (RB + Select) lists every combo of the current screen.
- **Key repeat**: 250 ms before repeating, then 66 ms (`KEYDELAY`, `KEYREPEAT` in `config.xml`); holding A + a direction speeds up to 40 ms after 6 repeats (`KEYREPEATFAST`) for long value sweeps. The cursor itself never accelerates, so it doesn't overshoot.
- **Every change is guarded by a sim test** (`key-grammar`, `live-mode`, `mixer-levels`).

## Crashes and logs

On the device the app writes `Applications/lgpt-rgnano.log` (the previous run is kept as `.log.prev`, capped at 1 MB with rotation to `.log.old`). Every 30 s a `[HEARTBEAT]` line records uptime and memory, so slow growth shows up. `[TRAIL]` lines are the recent keys, screens, dialogs and play/stop.

A crash (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT) appends a report to `Applications/lgpt-rgnano-crash.txt`: the commit, signal, fault address, `pc`/`lr`, return addresses found on the stack and the last 48 actions. The installer archives each build's ELF as `build/elf/<commit>.elf`, and

```
python tools/nano_crash_report.py        # card mounted as D:
```

names the functions and lines and prints the last heartbeats and log lines.

Soak test for crashes and leaks: `python tools/make_soak_script.py --minutes 15 --seed 1` writes a random-play script; run it with `tools/run-rgnano-sim.ps1 -Script projects/resources/RGNANO_SIM/soak.rgsim -Mute -OpenDemo Afterglow` and read the `[HEARTBEAT]` lines. The simulator's own crash handler writes `rgnano-sim-crash.txt` with the same action trail.

`DUMPEVENT` in `config.xml` logs every player tick and key: keep it `NO` on the device.

## Audio performance

**On the device.** The audio engine renders on its own thread, one buffer per sequencer tick. Its load is the time a buffer took to render (sequencer, every instrument, the buses, send effects, master EQ, limiter, master mix) over the time the buffer plays, smoothed; it is the number on the Mixer screen and in the heartbeat. The RG Nano has one core, so this wall time also counts any moment the screen's thread had the CPU while a buffer was being rendered. The heartbeat therefore logs both:

```
[HEARTBEAT] up 60s mem 10192KB view song player on load peak 52% (smoothed 50% cpu 44%) worst buffer 61%: seq 1 ch1 6 ch2 6 ... fx 5 meq 2 lim 3 mst 2
```

- `load peak`: the highest load the Mixer screen showed (read once a beat); `smoothed` the highest smoothed load, from wall time; `cpu` the same from the render thread's own CPU time. `smoothed` well above `cpu` means other threads took the CPU while audio was rendering.
- `worst buffer`: the slowest buffer since the last line, with where its time went, in percent of that buffer's play time: `seq` sequencer, `ch1`..`ch8` each track's instruments (sends and instrument EQ included), `bus` the channel buses (sum, clip, meters), `fx` reverb/echo/chorus, `meq` master EQ, `lim` limiter, `mst` master mix. (`Services/Audio/AudioProfiler.*`, a few clock reads per buffer.)
- The player's play-position updates (cursor marks, mini waveform, Mixer meters) are drawn by the UI thread: the audio thread only queues them (`AppWindow::queuePlayerUpdate`), so it never waits for the screen.

**In the ARM harness.** `tools/dsp-harness/engine_room_check.cpp` plays the *Engine Room* demo through the real player and mixer of the device build and measures it with `a7cost`, a qemu plugin (`tools/dsp-harness/qemu-plugin/`) that counts executed instructions weighted by their Cortex-A7 cost and charges them to the parts above. Unlike qemu's wall time (every float operation is emulated in software there) this is exact and repeatable, so it guards against regressions: `run-all.sh` fails when the song costs 25% more than the recorded figure. The first run builds the plugin-enabled qemu in `~/qemu-plugin` (a few minutes, no root).

```
wsl bash tools/dsp-harness/run.sh tools/dsp-harness/engine_room_check.cpp
wsl python3 tools/dsp-harness/a7profile.py --top 30            # cost per function
wsl python3 tools/dsp-harness/a7profile.py --lines fm4Render   # ... per source line
python tools/dsp-harness/compare_wav.py before.wav projects/buildRGNANO/harness/engine_room.wav
```

The check writes the mix to `projects/buildRGNANO/harness/engine_room.wav`; `compare_wav.py` gives the difference level (dBFS) and both spectra per octave, to prove an optimisation left the sound alone. `ENGINE_ROOM_WORST=10` lists the ten most expensive buffers part by part.

Rules that paid off in the DSP code: work a control block (16 samples) at a time and stage by stage, not every stage per sample; keep per-sample libm calls (`sin`, `pow`, `exp`), divisions and double-precision maths out of inner loops (tables, cached values, multiplies by a reciprocal); copy members used in a loop to locals (a store through a `float *` can alias them); NEON four samples at a time where the samples do not depend on each other (resampler, FM operators without feedback, saw ramps, buses, voice output). Where a change keeps the arithmetic, the render must stay bit-identical; where it changes it (HyperSynth's integer saw phase), `hyper_saw_check.cpp` shows the sound is the same.

## Code map

| Area | Where |
| --- | --- |
| Synth engine and presets | `sources/Application/Instruments/SynthInstrument.*` |
| Macro synth (voice, resampler, presets) | `sources/Application/Instruments/MacroInstrument.*`; its oscillator (Braids port, MIT) in `sources/Externals/Braids`; pages in `sources/Application/Views/InstrumentViewMacro.cpp`; ARM check `tools/dsp-harness/macro_check.cpp` |
| FM4 / HYPER / WAV engines (DSP) | `sources/Application/Instruments/SynthEngines.*` (ARM check: `tools/dsp-harness/synth_engines_check.cpp`) |
| Engine SOUND pages (grid, pictures, help) | `sources/Application/Views/InstrumentViewEngines.cpp` |
| Shared reverb / echo | `sources/Application/Mixer/SendFX.*` |
| Master EQ, instrument EQ design (shared biquads) | `sources/Application/Mixer/ThreeBandEQ.*`, `MasterEQ.*` |
| Master limiter (look-ahead) | `sources/Application/Mixer/MasterLimiter.*`, screen `Views/LimiterView.cpp` |
| Instrument EQ (per voice) | `sources/Application/Instruments/InstrumentEQ.*`, page `Views/InstrumentViewEQ.cpp` |
| Sampler play modes (forward, reverse, loops, ping-pong, osc) | `sources/Application/Instruments/SampleInstrument.*` (`setupVoicePlayback`, `Render`) |
| Sample editor (normalize, crop, fades, reverse, trim) | `sources/Application/Instruments/SampleProcessor.*`, `Views/ModalDialogs/SampleEditDialog.*` |
| Instrument bank, type switching, starter kit | `sources/Application/Instruments/InstrumentBank.cpp` |
| Synth screen | `sources/Application/Views/InstrumentViewSynth.cpp` |
| Helper overlay | `sources/Application/Views/BaseClasses/View.cpp` |
| Simulator script runner | `sources/Adapters/SDL/GUI/SDLEventManager.cpp` |
| Simulator boot / headless | `sources/Adapters/RGNANO_SIM/` |

## Publishing this guide

The guide source is `docs/rgnano-wiki/*.md` (plain Markdown, also valid as a GitHub wiki).

- `python tools/build_guide_site.py` renders it into a static site in `build/guide-site`.
- `python tools\publish_guide_site.py --gh-user <account>` builds it and publishes to GitHub Pages (`gh-pages` branch): https://inkmortal.github.io/LittleGPTracker-RG-Nano/
- `tools\publish-wiki.ps1` mirrors the same pages into a GitHub wiki, if the repository uses one.
