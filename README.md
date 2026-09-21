# LittleGPTracker for RG Nano

![Piggy](https://avatars.githubusercontent.com/u/180156201?s=400&u=ebb53bdea61a025edce0c3782ac75b532dd65dd7&v=4)

This is an experimental **LittleGPTracker / Little Piggy Tracker** fork for the **Anbernic RG Nano**.

The goal is to elevate the RG Nano into a tiny, serious tracker workstation comparable in creative ambition to the **Dirtywave M8**: compose with LGPT-style sequencing, import and manage samples, refine the 240x240 interface for the actual device, and test changes through a desktop simulator before copying builds to hardware.

This is not a Dirtywave M8 firmware clone. It is a native RG Nano fork of LGPT aimed at a similar pocket-production role: the Nano acts as the final arrangement and production box for sketches made on small devices such as a PO-33, chord/synth gadgets, or prepared sample packs.

## What's New: Synths, FX, Demo Songs

- **Native synth engine** (M8-style): 7 waveforms, sub, noise, FM, one-note chords (`CHRD`), ADSR, pitch envelope, glide, resonant filter with envelope, drive, LFO, 17 presets. New projects start with a 16-instrument synth kit, so you can make a full track with zero samples.
- **Send effects**: shared reverb and tempo-synced echo; every synth has reverb/delay sends.
- **Synth screen** with live pictures (waveform, envelope, filter curve, LFO) and plain-English help for the focused knob.
- **Demo songs** in `projects/resources/demos` (synthwave, wuxia, lo-fi, chiptune) composed with `tools/lgpt_composer.py`.
- **Beginner guide**: [docs/TRACKER_BASICS.md](docs/TRACKER_BASICS.md). The `RB + Select` helper now has a third "HOW TO" page on every screen.

## Current Focus

- native RG Nano builds and OPK packaging
- RG Nano-friendly project, song, instrument, and sample workflows
- audio input / sampling experiments where the hardware and OS allow it
- sample upload/import workflow for cases where live recording is not available
- native WAV rendering: `Stereo` creates `mixdown.wav`, `Stems` creates per-channel `channelN.wav` files
- 240x240 UI refinements for the RG Nano screen
- desktop `RGNANO_SIM` build for scripted smoke tests and screenshot capture

## Current Status

- Public experimental RG Nano fork, not a polished release.
- RG Nano-specific code exists for packaging, UI layout, audio capture experiments, and simulator support.
- The simulator can build on Windows with MSYS2/MinGW, run scripted input, write logs, and capture SDL screenshots.
- Hardware testing is still required before treating any build as performance-ready.

For the simulator, see [docs/RGNANO_SIM.md](docs/RGNANO_SIM.md).
For the RG Nano user manual, see [docs/RGNANO_USER_MANUAL.md](docs/RGNANO_USER_MANUAL.md).

## RG Nano Install

The RG Nano hardware target builds an ELF named `lgpt-rgnano.elf`. Runtime resources live in:

```text
projects/resources/RGNANO/
```

That folder contains:

- `config.xml` - RG Nano config, 240x240 screen, sample/project paths, key mapping
- `lgpt.funkey-s.desktop` - launcher entry for FunKey-style menus
- `lgpt.png` - launcher icon
- `INSTALL_HOW_TO.txt` - short device install notes

To install a build on the RG Nano:

1. Build or download `lgpt-rgnano.opk` for the normal FunKey/RG Nano launcher path.
2. Copy `lgpt-rgnano.opk` to the SD card's `Native games` folder.
3. For loose ELF testing, copy `lgpt-rgnano.elf` plus everything from `projects/resources/RGNANO/` into `/mnt/Applications/LGPT`.
4. Create these folders on the SD card if they do not already exist:

```text
/mnt/Applications/Tracks
/mnt/Applications/Samples
```

5. Copy the demo songs (`Tracks/lgpt_*` in the build zip, or `projects/resources/demos/lgpt_*`) into `/mnt/Applications/Tracks`. They open from the project list like any saved song.
6. Optional: put `.wav` samples in `/mnt/Applications/Samples`.
7. Launch `LGPT RG Nano` from the RG Nano app menu.

The default RG Nano config uses:

```xml
<ROOTFOLDER value="/mnt/Applications/Tracks"/>
<SAMPLELIB value="/mnt/Applications/Samples"/>
<SCREENMULT value="1"/>
<CHANNELMODE value="SPLIT"/>
```

Use `SPLIT` mode to see all 8 channels on the square screen, or change `CHANNELMODE` to `PAGED` for a roomier 4-channel-at-a-time view.

## RG Nano Controls

The fork keeps LGPT controls but adds RG Nano screen/layout behavior.

| Action | Control |
| --- | --- |
| Toggle split/paged channel view | `L + Select` |
| Paged mode, show channels 1-4 | `L + Left` |
| Paged mode, show channels 5-8 | `L + Right` |
| Confirm / primary action | `A` |
| Back / secondary action | `B` |
| Navigation | D-pad |
| Universal helper/minimap | `RB + Select` |
| Helper page switch | `Up` / `Down` while helper is open |
| Command picker | `Select` on Phrase/Table command columns |
| Sample import | `Select` on Instrument Source `sample` |
| Trim-aware root suggestion | `Select` on Instrument Source `root` |
| Sample trim marker select | `LB + Up/Down` on Instrument Source/Loop |
| Sample trim marker nudge | `LB + A + Left/Right` on Instrument Source/Loop |
| Source-row quick trim | `A + D-pad` on Instrument Source `sample` row |
| Source-row fast trim | `RB + A + Left/Right` on Instrument Source `sample` row |
| Preview current sample trim | `Start` on Instrument sample pages |
| Toggle sample preview mode | `RB + Start` on Instrument Source/Loop |
| Play/stop current context | `Start` |
| Play/stop in song context | `RB + Start` on Chain/Phrase-style screens |
| App exit/debug menu | Menu/Power button |

The app shows a small playback-scope label on main views: `STOP`, `PLAY:SONG`, `PLAY:CHAIN`, `PLAY:PHR`, `PLAY:LIVE`, or `AUDITION`. Use it to confirm whether you are hearing the full arrangement or only the current chain/phrase.

Hardware diagnostics are written to `/mnt/Applications/lgpt-rgnano.log`. The log includes input replay lines, screen state, and selected-field state so hardware sessions can be reproduced in the simulator. On the current RG Nano OS path, physical FN/Select reports as `KEY_O` in hardware logs; the desktop simulator keeps `k` for Select.

To bounce audio, open Project View, set `Render:` to `Stereo` or `Stems`, press `Start` to render/play, then press `Start` again to stop and close the WAV file. Outputs are written into the active project folder.

## Building For RG Nano

The RG Nano make target is:

```bash
cd projects
make PLATFORM=RGNANO
```

It expects the FunKey SDK at:

```text
sdk/FunKey-sdk-DrUm78
```

The target uses `projects/Makefile.RGNANO` and outputs:

```text
projects/lgpt-rgnano.elf
```

This hardware build path is still being tightened. If you only want to work on the UI, workflow, and simulator right now, use the simulator flow below.

## Simulator Quick Start

Build the simulator:

```powershell
.\run.ps1 -Task dev -NoSetup
```

Run the simulator smoke test:

```powershell
.\run.ps1 -Task smoke -NoSetup
```

The smoke test boots the RG Nano simulator, waits for the project screen, captures `smoke-boot.bmp`, and exits.

The simulator uses the same 240x240 screen contract and RG Nano key names, so it is the safest place to test UI and workflow changes before putting a build on the device.

## Sample Workflow

The RG Nano does not have a built-in microphone. This fork is therefore designed around two paths:

- copy prepared samples to `/mnt/Applications/Samples`
- experiment with audio input only where the RG Nano firmware/kernel exposes a usable capture device

The practical production workflow today is:

1. Make sounds on another device, such as a PO-33, chord gadget, synth, phone, or computer.
2. Export or record those sounds as `.wav` files.
3. Copy the samples into `/mnt/Applications/Samples` on the RG Nano SD card.
4. Arrange, sequence, edit, and finish the track inside LGPT on the RG Nano.

## About This Fork

This repository is a fork of [djdiskmachine/LittleGPTracker](https://github.com/djdiskmachine/LittleGPTracker), which is itself derived from the original work by `m-.-n`, aka [Marc Nostromo](https://github.com/Mdashdotdashn/LittleGPTracker).

That upstream work made this RG Nano experiment possible. This fork keeps the upstream license notices and adds project-specific RG Nano changes on top.

## License

See [LICENSE](LICENSE). In short:

- the original LittleGPTracker lineage includes BSD 3-Clause terms
- this project carries GPLv3 terms for the current fork
- this is free/open-source software, so forks, experiments, and redistribution are welcome under the actual license terms

Back up your tracker projects and samples before testing experimental builds on hardware.

## Releases

Latest upstream Little Piggy Tracker releases:

- [djdiskmachine/LittleGPTracker releases](https://github.com/djdiskmachine/LittleGPTracker/releases)

RG Nano builds from this fork are experimental. Check this repository's releases or build from source when no release is available.

Archived 2006 builds:

- [Stable & Ghetto](https://github.com/NinjasCL-archive/LittleGPTracker/releases/tag/v1)

## Documentation

General Little Piggy Tracker documentation lives in [docs](docs).

Recommended reading:

- [RG Nano User Manual](docs/RGNANO_USER_MANUAL.md)
- [RG Nano Input Map](docs/RGNANO_INPUT_MAP.md)
- [What is Little Piggy Tracker](docs/wiki/What-is-LittlePiggyTracker.md)
- [Quick-Start Guide](docs/wiki/quick_start_guide.md)
- [Little Piggy Tracker Configuration](docs/LittlePiggyTrackerConf.md)
- [Tips and Tricks](docs/wiki/tips_and_tricks.md)
- [RG Nano Simulator](docs/RGNANO_SIM.md)

## Platform Notes

This fork inherits upstream platform support for desktop and handheld builds. The RG Nano-specific work is experimental and may move faster than upstream release branches.

The upstream tracker also supports platforms such as Windows, macOS, Linux, PSP, Miyoo Mini, RG35XX Plus, and other handheld targets. MIDI, SoundFont, and PrintFX support vary by platform and depend on the OS/audio stack.
