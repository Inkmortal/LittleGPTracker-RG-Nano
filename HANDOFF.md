# Handoff: open bugs and where things stand (2026-09-26)

For the next agent picking up this repo. Read `AGENTS.md` (collaboration contract) and
`docs/RGNANO_SIM.md` (simulator) first. The user is the tester: never ask them to run
commands, copy logs or inspect anything. Ask only for normal use, e.g. "plug in the Nano and say mounted".

## State

- Branch `feature/sample-workstation-ui`, pushed to `origin` and mirrored to `main`.
  Push without changing the active gh account:
  `TOKEN=$(gh auth token -u Inkmortal); B64=$(printf "x-access-token:%s" "$TOKEN" | base64 -w0); git -c credential.helper= -c "http.extraheader=Authorization: Basic $B64" push -q origin HEAD HEAD:main && git branch -f main HEAD`
- **The Nano runs f72fba4.** Not yet installed: 7ba119c (Select on sample pages), 102c545 (A on
  `sample none` opens the browser), 0833548 (helper overlay fix). Install when the card is mounted
  as `D:`: `powershell -File tools/install-rgnano.ps1` (builds, runs ARM checks, archives the ELF). Don't eject the card afterwards.
- Build the sim with `powershell -File tools/build-rgnano-sim.ps1` (`make` isn't on PATH in Git Bash).
  Run one script: `tools/run-rgnano-sim.ps1 -Script <rgsim> -Mute [-ResetLastProject | -OpenDemo=Name]`.
  Full suite: `tools/run-rgnano-sim-suite.ps1` (~20 min). Run it in the background, and never run two sim jobs at once (they share the exe and the log).

## Open bugs the user reported (device, f72fba4)

1. **Select on `sample none` does nothing** (Instrument screen after setting type to sample by hand).
   In the sim, the same code opens the sample browser, both by hand and in the tests
   `sample-type-by-hand` and `sample-select`. So the device differs.
   - The user thinks the modal may open but not show. Check the device log: `View.cpp` writes a
     `TRAIL keys %04X view N` line per press and `TRAIL dialog open over view N` when a modal opens.
     That tells you whether Select (EPBM_SELECT = 0x0200) arrived and whether a dialog opened.
   - Possibly related: views draw their own play-position updates even while a modal is open
     (`View::OnPlayerUpdate` forwards to the modal, then subclasses paint their meters or markers). If a
     song is playing, the view may paint over the dialog. Verify with an `expect_screens_same` check
     while a dialog is open during playback.
2. **Meters drawn over the RB+Select helper.** Root cause found and fixed in 0833548: since
   5fc8f14, audio-thread player updates are queued and drawn by `AppWindow::drawPendingPlayerUpdates`,
   which skipped the helper check. The sim took the unqueued path, so the old test missed it. Now both paths go
   through `drawPlayerUpdate`. `helper-over-playback` uses `expect_screens_same` and was
   verified to fail on the old code. **Not yet confirmed on the device.**
3. **Crashes after a while of use; the latest was on the Chain screen.** Read the report on the card:
   `python tools/nano_crash_report.py` (symbolizes with `build/elf/<commit>.elf`), plus
   `Applications/lgpt-rgnano.log` / `.prev` (the TRAIL breadcrumbs show the last actions). Suspect
   races introduced by the queued player updates in 5fc8f14: the queue drains on the UI thread under
   `drawMutex_`, and the view may change or be deleted between queueing and drawing. Also check
   `UndoHistory` / instrument type changes (dangling Variable pointers were an earlier crash class).
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
