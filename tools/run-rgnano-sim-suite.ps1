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
    Name = "looping-sample-stops"
    Script = "looping-sample-stops.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture")
  },
  @{
    Name = "a-press-does-not-climb"
    Script = "a-press-does-not-climb.rgsim"
    Args = @("-ResetLastProject")
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
    Name = "wuxia-lofi-studio"
    Script = "wuxia-lofi-studio.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture", "-Skin")
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
    $caseParams[$flag.TrimStart("-")] = $true
  }
  if (-not $Audible) {
    $caseParams["Mute"] = $true
  }
  & $runner @caseParams
  $exitCode = $LASTEXITCODE
  $caseEnded = Get-Date
  Get-ChildItem -LiteralPath $root -File -Include "*.bmp","*.wav" -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $caseStarted } |
    Remove-Item -Force
  $results += [pscustomobject]@{
    name = $case.Name
    script = $case.Script
    exitCode = $exitCode
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

$summaryPath = Join-Path $ArtifactsRoot "suite-summary.json"
$summary | ConvertTo-Json -Depth 5 | Out-File -LiteralPath $summaryPath -Encoding utf8

if ($summary.failed -gt 0) {
  Write-Error "RG Nano simulator suite failed. See $summaryPath"
  exit 1
}

Write-Host "RG Nano simulator suite passed: $($summary.passed) cases in $($summary.durationSeconds)s"
Write-Host "Summary: $summaryPath"
exit 0
