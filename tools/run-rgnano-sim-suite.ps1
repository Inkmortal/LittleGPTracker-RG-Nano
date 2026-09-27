param(
  [string]$ArtifactsRoot = ".\sim-artifacts-suite",
  [switch]$NoBuild,
  [switch]$StopOnFailure,
  [switch]$Audible,
  [string]$Only = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$runner = Join-Path $PSScriptRoot "run-rgnano-sim.ps1"
$builder = Join-Path $PSScriptRoot "build-rgnano-sim.ps1"
$scriptRoot = Join-Path $root "projects\resources\RGNANO_SIM"

if (-not $NoBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File $builder
  if ($LASTEXITCODE -ne 0) {
    throw "RG Nano simulator build failed with exit code $LASTEXITCODE"
  }
}

New-Item -ItemType Directory -Force -Path $ArtifactsRoot | Out-Null

$suite = @(
  @{
    Name = "smoke"
    Script = "smoke.rgsim"
    Args = @()
  },
  @{
    Name = "new-project-route"
    Script = "new-project-route.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "sample-fixture"
    Script = "sample-fixture.rgsim"
    Args = @("-Skin", "-SeedSampleFixture")
  },
  @{
    Name = "sample-import-workflow"
    Script = "sample-import-workflow.rgsim"
    Args = @("-ResetLastProject", "-SeedSampleFixture")
  },
  @{
    Name = "instrument-start-trim-preview"
    Script = "instrument-start-trim-preview.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
  },
  @{
    Name = "basic-music-workflow"
    Script = "basic-music-workflow.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "demo-song-workflow"
    Script = "demo-song-workflow.rgsim"
    Args = @("-ResetLastProject", "-SeedSampleFixture", "-Skin")
  },
  @{
    Name = "all-8-channels-workflow"
    Script = "all-8-channels-workflow.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
  },
  @{
    Name = "mixer-waveform-workflow"
    Script = "mixer-waveform-workflow.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
  },
  @{
    Name = "song-mini-waveform-workflow"
    Script = "song-mini-waveform-workflow.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
  },
  @{
    Name = "note-spelling-workflow"
    Script = "note-spelling-workflow.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "scale-key-workflow"
    Script = "scale-key-workflow.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "scale-free-override-workflow"
    Script = "scale-free-override-workflow.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "lofi-sample-pack"
    Script = "lofi-sample-pack.rgsim"
    Args = @("-SeedLofiFixture")
  },
  @{
    Name = "producer-navigation-tour"
    Script = "producer-navigation-tour.rgsim"
    Args = @("-ResetLastProject", "-Skin")
  },
  @{
    Name = "producer-persistence-create"
    Script = "producer-persistence-create.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
  },
  @{
    Name = "producer-persistence-reopen"
    Script = "producer-persistence-reopen.rgsim"
    Args = @("-Skin")
  },
  @{
    Name = "song-tools-create"
    Script = "song-tools-create.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "song-tools-reopen"
    Script = "song-tools-reopen.rgsim"
    Args = @()
  },
  @{
    Name = "scale-editor"
    Script = "scale-editor.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "sequencer-commands"
    Script = "sequencer-commands.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "power-menu-input-isolation"
    Script = "power-menu-input-isolation.rgsim"
    Args = @("-ResetLastProject", "-Skin")
  },
  @{
    Name = "synth-starter-kit"
    Script = "synth-starter-kit.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "synth-instrument-pages"
    Script = "synth-instrument-pages.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "macro-synth"
    Script = "macro-synth.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "macro-synth-reopen"
    Script = "macro-synth-reopen.rgsim"
    Args = @()
  },
  @{
    Name = "looping-sample-stops"
    Script = "looping-sample-stops.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture")
  },
  @{
    Name = "sample-processing"
    Script = "sample-processing.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture")
  },
  @{
    Name = "a-press-does-not-climb"
    Script = "a-press-does-not-climb.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "instrument-list"
    Script = "instrument-list.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "instrument-list-demo"
    Script = "instrument-list-demo.rgsim"
    Args = @("-OpenDemo=JadeSword")
  },
  @{
    Name = "song-list-order"
    Script = "song-list-order.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "mixer-levels"
    Script = "mixer-levels.rgsim"
    Args = @("-OpenDemo=NeonDrive")
  },
  @{
    Name = "eq-screen"
    Script = "eq-screen.rgsim"
    Args = @("-OpenDemo=NeonDrive")
  },
  @{
    Name = "master-limiter"
    Script = "master-limiter.rgsim"
    Args = @("-OpenDemo=NeonDrive")
  },
  @{
    Name = "engine-room-load"
    Script = "engine-room-load.rgsim"
    Args = @("-OpenDemo=EngineRoom")
  },
  @{
    Name = "instrument-eq"
    Script = "instrument-eq.rgsim"
    Args = @("-OpenDemo=JadeSword")
  },
  @{
    Name = "random-tools"
    Script = "random-tools.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "help-topics"
    Script = "help-topics.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "guide-navigation"
    Script = "guide-navigation.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "guide-pages"
    Script = "guide-pages.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "guide-over-playback"
    Script = "guide-over-playback.rgsim"
    Args = @("-OpenDemo=EngineRoom")
  },
  @{
    Name = "command-selector-workflow"
    Script = "command-selector-workflow.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "helper-over-playback"
    Script = "helper-over-playback.rgsim"
    Args = @("-OpenDemo=EngineRoom")
  },
  @{
    Name = "sample-select"
    Script = "sample-select.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "sample-type-by-hand"
    Script = "sample-type-by-hand.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "undo"
    Script = "undo.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "key-grammar"
    Script = "key-grammar.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    # B backs out one layer from every dialog, menu and overlay
    Name = "back-out-everywhere"
    Script = "back-out-everywhere.rgsim"
    Args = @("-ResetLastProject", "-SeedSamplePacks", "-NameSeed=7")
  },
  @{
    Name = "chain-warp"
    Script = "chain-warp.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "picked-instrument"
    Script = "picked-instrument.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "sound-files"
    Script = "sound-files.rgsim"
    Args = @("-ResetLastProject", "-SeedSampleFixture")
  },
  @{
    # Right after sound-files: its template kit is what a new song gets
    Name = "sound-template-new-song"
    Script = "sound-template-new-song.rgsim"
    Args = @("-ResetLastProject", "-KeepSounds")
  },
  @{
    Name = "rack"
    Script = "rack.rgsim"
    Args = @("-ResetLastProject", "-SeedSampleFixture")
  },
  @{
    Name = "rack-compose"
    Script = "rack-compose.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "live-mode"
    Script = "live-mode.rgsim"
    Args = @("-OpenDemo=Afterglow")
  },
  @{
    # Dumps the Afterglow demo for first-song-walkthrough (keep them in order)
    Name = "first-song-reference"
    Script = "first-song-reference.rgsim"
    Args = @("-OpenDemo=Afterglow")
  },
  @{
    # Every press of "Your First Song" (tools/make_walkthrough.py), from a
    # fresh start; the song it ends with must equal the Afterglow demo
    Name = "first-song-walkthrough"
    Script = "first-song-walkthrough.rgsim"
    Args = @("-ResetLastProject", "-SeedSamplePacks", "-NoKeyRepeat", "-NameSeed=7")
  },
  @{
    Name = "fx-screen"
    Script = "fx-screen.rgsim"
    Args = @("-OpenDemo=NeonDrive")
  },
  @{
    Name = "render-to-sample"
    Script = "render-to-sample.rgsim"
    Args = @("-OpenDemo=NeonDrive")
  },
  @{
    Name = "mod-slots"
    Script = "mod-slots.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "mod-slots-legacy"
    Script = "mod-slots-legacy.rgsim"
    Args = @("-OpenDemo=LateLibrary")
  },
  @{
    Name = "mod-slots-reopen"
    Script = "mod-slots-reopen.rgsim"
    Args = @()
  },
  @{
    Name = "demo-stop-silences"
    Script = "demo-stop-silences.rgsim"
    Args = @("-OpenDemo=JadeSword")
  },
  @{
    Name = "start-stops-everything"
    Script = "start-stops-everything.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "audition-stuck-note"
    Script = "audition-stuck-note.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "start-screen-ux"
    Script = "start-screen-ux.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "beginner-first-beat"
    Script = "beginner-first-beat.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "synth-preset-tour"
    Script = "synth-preset-tour.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "synth-engines"
    Script = "synth-engines.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "synth-drum-phys"
    Script = "synth-drum-phys.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "wuxia-lofi-studio"
    Script = "wuxia-lofi-studio.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
  },
  @{
    Name = "sample-too-long"
    Script = "sample-too-long.rgsim"
    Args = @("-ResetLastProject", "-SeedLongSample")
  }
)

$results = @()
$started = Get-Date

if ($Only) {
  $wanted = $Only.Split(",")
  $suite = $suite | Where-Object { $wanted -contains $_.Name }
}

foreach ($case in $suite) {
  $caseStarted = Get-Date
  $caseArtifacts = Join-Path $ArtifactsRoot $case.Name
  $scriptPath = Join-Path $scriptRoot $case.Script
  Write-Host "==> $($case.Name)"

  # Run the case in this PowerShell process: spawning powershell.exe per case
  # opens console windows that steal focus.
  $caseParams = @{ Script = $scriptPath; ArtifactsDir = $caseArtifacts }
  foreach ($flag in $case.Args) {
    # "-Switch" or "-Name=Value"
    $parts = $flag.TrimStart("-").Split("=", 2)
    if ($parts.Count -eq 2) {
      $caseParams[$parts[0]] = $parts[1]
    } else {
      $caseParams[$parts[0]] = $true
    }
  }
  if (-not $Audible) {
    $caseParams["Mute"] = $true
  }
  & $runner @caseParams
  $exitCode = $LASTEXITCODE
  $caseEnded = Get-Date
  # Screenshots and captures a case left in the repo root (-Include is
  # ignored with -LiteralPath in PowerShell 5.1: it matched every file)
  Get-ChildItem -LiteralPath $root -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in ".bmp", ".wav" -and $_.LastWriteTime -ge $caseStarted } |
    Remove-Item -Force
  # Most the case held, as the RG Nano's RSS would show (RGNanoSimMemory)
  $memoryPeakKB = $null
  $caseLog = Join-Path $caseArtifacts "rgnano-sim.log"
  if (Test-Path -LiteralPath $caseLog) {
    $peakLine = Select-String -LiteralPath $caseLog -Pattern "memory peak (\d+)KB device RSS" | Select-Object -Last 1
    if ($peakLine) {
      $memoryPeakKB = [int]$peakLine.Matches[0].Groups[1].Value
      Write-Host "    memory peak $memoryPeakKB KB"
    }
  }
  $results += [pscustomobject]@{
    name = $case.Name
    script = $case.Script
    exitCode = $exitCode
    memoryPeakKB = $memoryPeakKB
    startedAt = $caseStarted.ToString("o")
    durationSeconds = [math]::Round(($caseEnded - $caseStarted).TotalSeconds, 2)
    artifacts = (Resolve-Path -LiteralPath $caseArtifacts -ErrorAction SilentlyContinue).Path
  }

  if ($exitCode -ne 0) {
    Write-Host "    FAILED (exit $exitCode) - see $caseArtifacts"
    if ($StopOnFailure) {
      break
    }
  }
}

$ended = Get-Date
$summary = [pscustomobject]@{
  startedAt = $started.ToString("o")
  endedAt = $ended.ToString("o")
  durationSeconds = [math]::Round(($ended - $started).TotalSeconds, 2)
  passed = @($results | Where-Object { $_.exitCode -eq 0 }).Count
  failed = @($results | Where-Object { $_.exitCode -ne 0 }).Count
  results = $results
}

$heaviest = $results | Where-Object { $_.memoryPeakKB } | Sort-Object memoryPeakKB -Descending | Select-Object -First 5
if ($heaviest) {
  Write-Host "Heaviest cases (device RSS KB): $(($heaviest | ForEach-Object { "$($_.name) $($_.memoryPeakKB)" }) -join ', ')"
}

$summaryPath = Join-Path $ArtifactsRoot "suite-summary.json"
$summary | ConvertTo-Json -Depth 5 | Out-File -LiteralPath $summaryPath -Encoding utf8

if ($summary.failed -gt 0) {
  Write-Error "RG Nano simulator suite failed. See $summaryPath"
  exit 1
}

Write-Host "RG Nano simulator suite passed: $($summary.passed) cases in $($summary.durationSeconds)s"
Write-Host "Summary: $summaryPath"
exit 0
