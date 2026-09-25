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

Assertions: `expect_view`, `expect_screen_text`, `expect_selected_text`, `expect_player_running`, `expect_play_mode`, `expect_audio_activity`, `expect_song_chain`, `expect_phrase_row_count`, `expect_instrument_type/name/param`, `expect_size 240 240`, `expect_no_error`, and more — see `docs/RGNANO_SIM.md`.

State setup for long scenarios: `sim_set_synth`, `sim_set_instrument_param`, `sim_set_song_chain`, `sim_set_chain_phrase`, `sim_set_phrase_note`, `sim_set_phrase_command`, `sim_set_tempo`.

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

Soak test for crashes and leaks: `python tools/make_soak_script.py --minutes 15 --seed 1` writes a random-play script; run it with `tools/run-rgnano-sim.ps1 -Script projects/resources/RGNANO_SIM/soak.rgsim -Mute -OpenDemo Dusk` and read the `[HEARTBEAT]` lines. The simulator's own crash handler writes `rgnano-sim-crash.txt` with the same action trail.

`DUMPEVENT` in `config.xml` logs every player tick and key: keep it `NO` on the device.

## Code map

| Area | Where |
| --- | --- |
| Synth engine and presets | `sources/Application/Instruments/SynthInstrument.*` |
| Macro synth (voice, resampler, presets) | `sources/Application/Instruments/MacroInstrument.*`; its oscillator (Braids port, MIT) in `sources/Externals/Braids`; pages in `sources/Application/Views/InstrumentViewMacro.cpp`; ARM check `tools/dsp-harness/macro_check.cpp` |
| Shared reverb / echo | `sources/Application/Mixer/SendFX.*` |
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
