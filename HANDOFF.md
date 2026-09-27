# Handoff: open bugs and where things stand (2026-09-26)

For the next agent picking up this repo. Read `AGENTS.md` (collaboration contract) and
`docs/RGNANO_SIM.md` (simulator) first. The user is the tester: never ask them to run
commands, copy logs or inspect anything. Ask only for normal use, e.g. "plug in the Nano and say mounted".

## State

- Branch `feature/sample-workstation-ui`, pushed to `origin` and mirrored to `main`.
  Push without changing the active gh account:
  `TOKEN=$(gh auth token -u Inkmortal); B64=$(printf "x-access-token:%s" "$TOKEN" | base64 -w0); git -c credential.helper= -c "http.extraheader=Authorization: Basic $B64" push -q origin HEAD HEAD:main && git branch -f main HEAD`
- **The Nano runs dafec52** (installed 2026-09-27 01:38). Merged since, not yet on the Nano and
  **not yet verified by a full suite run** (the last run was killed by Windows low memory while 7
  agents were building): the action sweep (19 sweep-*.rgsim + 5 bug fixes), Joyride demo, the
  revamped demos (NeonDrive, JadeSword, PixelQuest, EngineRoom, SunsetClub), the preset loudness
  fix (preset_level_check in the dsp harness), the sidechain duck fix, notification row fixes.
  Next: run `tools/run-rgnano-sim-suite.ps1` alone, fix what fails, then install.
- The First Song walkthrough lost 3 steps after step 606 (new notes use the last-made sound), so the
  guide images after that step are off by 3: rerun `python tools/make_walkthrough.py` (full capture).
- Install when the card is mounted as `D:`: `powershell -File tools/install-rgnano.ps1` (builds, runs ARM checks, archives the ELF). Don't eject the card afterwards.
- GBA launchers (mGBA default + gpSP) with shared saves: `tools/rgnano-gba/install.ps1`.
- Decisions made with the user on 2026-09-27: Sound Rack (docs/INSTRUMENT_FIRST_WORKFLOW.md option A),
  X copy / Y paste / LB+Y duplicate, B always backs out one step (so B on "Save your work?" cancels;
  No+A leaves without saving). Afterglow stays the First Song; Joyride is an extra demo.
- M8 parity status and the ranked build list: docs/RGNANO_M8_SCREEN_CAPABILITY_AUDIT.md.
- Build the sim with `powershell -File tools/build-rgnano-sim.ps1` (`make` isn't on PATH in Git Bash).
  Run one script: `tools/run-rgnano-sim.ps1 -Script <rgsim> -Mute [-ResetLastProject | -OpenDemo=Name]`.
  Full suite: `tools/run-rgnano-sim-suite.ps1` (~20 min). Run it in the background, and never run two sim jobs at once (they share the exe and the log).

## Open bugs the user reported (device, f72fba4)

1. **Select on `sample none` does nothing** (Instrument screen after setting type to sample by hand).
   The sim opens the sample browser (tests `sample-type-by-hand`, `sample-select`). The two device
   logs from f72fba4 contain no Select-alone press (`keys 0200`) at all, so they can't confirm it;
   an older device log shows Select alone does reach views (`view-button mask=0x0200`). Fixes
   7ba119c/102c545 target this; re-test on the device after installing.
2. **Meters drawn over the RB+Select helper.** Root cause found and fixed in 0833548: since
   5fc8f14, audio-thread player updates are queued and drawn by `AppWindow::drawPendingPlayerUpdates`,
   which skipped the helper check. The sim took the unqueued path, so the old test missed it. Now both paths go
   through `drawPlayerUpdate`. `helper-over-playback` uses `expect_screens_same` and was
   verified to fail on the old code. **Not yet confirmed on the device.**
3. **Chain-screen "crash" was a freeze: fixed.** The log ended on `keys 0028 view 1` (B+Up on
   Chain) with no crash report. `ChainView::warpInColumn` moved the song cursor until it found a
   chain, and the cursor stops at the first/last song row, so with no chain that way it looped
   forever while holding the mixer lock. Now it scans the column once (`chain-warp` test).
   New: a hang watchdog (`CrashLog::StartWatchdog`, 30 s). A key press or its redraw that doesn't
   finish writes a crash report with a `hang` line and the stuck thread's stack, then quits
   (device: SIGQUIT to the stuck thread; sim: `RGNANO_SIM_HANG` in rgnano-sim-crash.txt, exit 3),
   so hangs show in `nano_crash_report.py` and fail sim tests instead of blocking them.
4. "A bunch of other stuff like that." The user will log more when back home.

## The user's main criticism: tests are too weak

The suites check that text exists (`expect_screen_text`) but not that the screen is *right*: nothing
drawn over overlays, every legal key on every screen does something visible, dialogs actually visible.
Also the sim can take different code paths from the device (threading, see bug 2). Next steps:
- Build a per-screen action sweep: for every view and every legal button or combo in `Controls.md`,
  press it and assert a visible effect (screenshot differs, or a named expectation), and assert
  overlays and dialogs stay pixel-stable during playback (`expect_screens_same`).
- Make the sim take the device's threaded player-update path (audio on its own thread, updates
  queued) so it can't hide device-only bugs.
- Prefer testing real key presses over the sim's `set`/`goto` shortcut goals.

## Useful facts

- Device libm: `floor()` is broken (replaced in `RGNANOLibm.cpp`), and `sin()` of large arguments is wrong, so wrap phases.
- Key grammar and every mapping: `docs/rgnano-wiki/Controls.md`. In-app guide: `tools/build_ingame_guide.py`.
- User preferences: talk short; don't overengineer; no fallbacks; don't steal focus (sims headless and muted).
