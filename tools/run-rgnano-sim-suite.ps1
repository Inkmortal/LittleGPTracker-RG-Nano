# Every simulator test, several at once.
#   run-rgnano-sim-suite.ps1 -Tier quick   # a few minutes: key workflows, every
#                                          # single key on every screen, golden audio
#   run-rgnano-sim-suite.ps1               # full: every case, every key combo
#   -Jobs N          simulators at once (default: half the cores, fewer if memory is short)
#   -UpdateGoldens   accept the current sounds and key results as the new goldens
#   -NoAudio         skip the golden audio renders
#   -Only a,b        just these cases
param(
  [string]$ArtifactsRoot = ".\sim-artifacts-suite",
  [switch]$NoBuild,
  [switch]$StopOnFailure,
  [switch]$Audible,
  [string]$Only = "",
  [ValidateSet("quick", "full")]
  [string]$Tier = "full",
  [int]$Jobs = 0,
  [switch]$NoAudio,
  [switch]$UpdateGoldens
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
    Name = "joyride-demo"
    Script = "joyride-demo.rgsim"
    Args = @("-OpenDemo=Joyride")
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
    Args = @("-ResetLastProject", "-SeedLofiFixture")
  },
  @{
    Name = "sample-type-by-hand"
    Script = "sample-type-by-hand.rgsim"
    Args = @("-ResetLastProject", "-SeedLofiFixture")
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
    Args = @("-ResetLastProject", "-SeedSamplePacks", "-NameSeed=7", "-SeedDemos")
  },
  @{
    Name = "chain-warp"
    Script = "chain-warp.rgsim"
    Args = @("-ResetLastProject")
  },
  @{
    Name = "copy-paste-xy"
    Script = "copy-paste-xy.rgsim"
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
    Args = @("-ResetLastProject", "-SeedDemos")
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

# Cases that need the one before them (a song it saved, a file it wrote)
# run right after it, in the same sandbox
$after = @{
  "producer-persistence-reopen" = "producer-persistence-create"
  "song-tools-reopen" = "song-tools-create"
  "macro-synth-reopen" = "macro-synth"
  "mod-slots-legacy" = "mod-slots"
  "mod-slots-reopen" = "mod-slots-legacy"
  "sound-template-new-song" = "sound-files"
  "first-song-walkthrough" = "first-song-reference"
}

# The quick tier: the workflows people use most, plus every single key on
# every screen (sweep-quick-*), the playback overlays and the golden audio
$quickCases = @(
  "smoke", "basic-music-workflow", "demo-song-workflow", "key-grammar",
  "back-out-everywhere", "copy-paste-xy", "chain-warp", "undo", "rack",
  "rack-compose", "picked-instrument", "sample-select", "sample-type-by-hand",
  "command-selector-workflow", "helper-over-playback", "guide-navigation",
  "start-screen-ux", "live-mode", "audition-stuck-note", "demo-stop-silences",
  "synth-drum-phys", "mixer-levels", "sequencer-commands", "power-menu-input-isolation"
)

# The key sweep (tools/sweep), regenerated from the table each run: every
# key and combo on every screen (full) or every single key (quick)
$sweepArgs = @()
if ($Tier -eq "quick") { $sweepArgs += "--quick" }
& python (Join-Path $PSScriptRoot "sweep\make_sweep.py") @sweepArgs | Out-Null
if ($LASTEXITCODE -ne 0) {
  throw "key sweep generation failed"
}
if ($Tier -eq "quick") {
  & python (Join-Path $PSScriptRoot "sweep\make_sweep.py") | Out-Null  # sweep-playback too
  $suite = @($suite | Where-Object { $quickCases -contains $_.Name })
  $sweepFilter = "sweep-quick-*.rgsim"
} else {
  $sweepFilter = "sweep-*.rgsim"
}
Get-ChildItem -LiteralPath $scriptRoot -Filter $sweepFilter | Sort-Object Name | ForEach-Object {
  if ($Tier -eq "full" -and $_.BaseName -like "sweep-quick-*") { return }
  $suite += @{
    Name = $_.BaseName
    Script = $_.Name
    Args = @("-OpenDemo=Afterglow")
  }
}
if ($Tier -eq "quick") {
  $suite += @{ Name = "sweep-playback"; Script = "sweep-playback.rgsim"; Args = @("-OpenDemo=Afterglow") }
}

if ($Only) {
  $wanted = $Only.Split(",")
  $suite = @($suite | Where-Object { $wanted -contains $_.Name })
}

# Chains: a case and the ones that need it, in order
$chains = New-Object System.Collections.ArrayList
$chainOf = @{}
foreach ($case in $suite) {
  $dep = $after[$case.Name]
  if ($dep -and $chainOf.ContainsKey($dep)) {
    $chain = $chainOf[$dep]
    [void]$chain.Add($case)
  } else {
    $chain = New-Object System.Collections.ArrayList
    [void]$chain.Add($case)
    [void]$chains.Add($chain)
  }
  $chainOf[$case.Name] = $chain
}
# Longest first (by the last run's times), so no long case starts last
$lastTimes = @{}
$lastSummary = Join-Path $ArtifactsRoot "suite-summary.json"
if (Test-Path -LiteralPath $lastSummary) {
  try {
    (Get-Content -LiteralPath $lastSummary -Raw | ConvertFrom-Json).results | ForEach-Object { $lastTimes[$_.name] = $_.durationSeconds }
  } catch {}
}
$chains = @($chains | Sort-Object -Descending { $t = 0; foreach ($c in $_) { if ($lastTimes[$c.Name]) { $t += $lastTimes[$c.Name] } else { $t += 60 } }; $t })

# How many simulators at once: half the cores by default, fewer when memory
# is short (each sim's Windows peak is measured; 200 MB assumed until then)
if ($Jobs -le 0) {
  $cores = [Environment]::ProcessorCount
  $Jobs = [math]::Max(1, [math]::Floor($cores / 2))
  $freeMB = [math]::Floor((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1024)
  $perSimMB = 200
  if (Test-Path -LiteralPath $lastSummary) {
    try {
      $peaks = (Get-Content -LiteralPath $lastSummary -Raw | ConvertFrom-Json).results | Where-Object { $_.hostPeakKB } | ForEach-Object { $_.hostPeakKB }
      if ($peaks) { $perSimMB = [math]::Max(64, [math]::Ceiling(($peaks | Measure-Object -Maximum).Maximum / 1024) * 2) }
    } catch {}
  }
  $byMemory = [math]::Max(1, [math]::Floor($freeMB * 0.5 / $perSimMB))
  $Jobs = [math]::Min($Jobs, $byMemory)
}
$sandboxRoot = Join-Path $root "build\sim-par"
Write-Host "Tier $Tier : $($suite.Count) cases in $($chains.Count) chains, $Jobs at once (sandboxes in $sandboxRoot)"

$started = Get-Date

# Golden audio first (it uses every core under qemu for a few minutes)
$audioExit = 0
if (-not $NoAudio -and -not $Only) {
  Write-Host "==> golden audio"
  $audioArgs = @((Join-Path $PSScriptRoot "golden_audio.py"))
  if ($UpdateGoldens) { $audioArgs += "--update" }
  & python @audioArgs | Select-Object -Last 12 | ForEach-Object { Write-Host "    $_" }
  $audioExit = $LASTEXITCODE
}

$chainScript = {
  param($cases, $runner, $scriptRoot, $artifactsRoot, $sandboxRoot, $slots, $audible, $root)
  $slot = 0
  while (-not $slots.TryDequeue([ref]$slot)) { Start-Sleep -Milliseconds 50 }
  try {
    $sandbox = Join-Path $sandboxRoot "w$slot"
    # Every chain starts like a fresh install: no songs, samples or sounds
    # left by another case
    $data = Join-Path $sandbox "rgnano-sim-data"
    if (Test-Path -LiteralPath $data) { Remove-Item -LiteralPath $data -Recurse -Force }
    foreach ($leftover in "last_project", "sweep-results.txt", "sweep-states.jsonl") {
      $f = Join-Path $sandbox $leftover
      if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $sandbox "sim-artifacts-suite") | Out-Null
    foreach ($case in $cases) {
      $caseStarted = Get-Date
      $caseArtifacts = Join-Path $artifactsRoot $case.Name
      $caseParams = @{ Script = (Join-Path $scriptRoot $case.Script); ArtifactsDir = $caseArtifacts; Sandbox = $sandbox
                       ExePath = (Join-Path $root "projects\lgpt-rgnano-sim.exe") }
      foreach ($flag in $case.Args) {
        $parts = $flag.TrimStart("-").Split("=", 2)
        if ($parts.Count -eq 2) { $caseParams[$parts[0]] = $parts[1] } else { $caseParams[$parts[0]] = $true }
      }
      if (-not $audible) { $caseParams["Mute"] = $true }
      & $runner @caseParams *> $null
      $exitCode = $LASTEXITCODE
      # Files a case left in the sandbox (screenshots, captures)
      Get-ChildItem -LiteralPath $sandbox -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in ".bmp", ".wav" } | Remove-Item -Force
      $memoryPeakKB = $null
      $caseLog = Join-Path $caseArtifacts "rgnano-sim.log"
      if (Test-Path -LiteralPath $caseLog) {
        $peakLine = Select-String -LiteralPath $caseLog -Pattern "memory peak (\d+)KB device RSS" | Select-Object -Last 1
        if ($peakLine) { $memoryPeakKB = [int]$peakLine.Matches[0].Groups[1].Value }
      }
      $hostPeakKB = $null
      $hostFile = Join-Path $caseArtifacts "host-peak-kb.txt"
      if (Test-Path -LiteralPath $hostFile) { $hostPeakKB = [int](Get-Content -LiteralPath $hostFile) }
      [pscustomobject]@{
        __case = $true
        name = $case.Name
        script = $case.Script
        exitCode = $exitCode
        memoryPeakKB = $memoryPeakKB
        hostPeakKB = $hostPeakKB
        slot = $slot
        startedAt = $caseStarted.ToString("o")
        durationSeconds = [math]::Round(((Get-Date) - $caseStarted).TotalSeconds, 2)
        artifacts = $caseArtifacts
      }
    }
  } finally {
    $slots.Enqueue($slot)
  }
}

$slots = New-Object 'System.Collections.Concurrent.ConcurrentQueue[int]'
for ($i = 0; $i -lt $Jobs; $i++) { $slots.Enqueue($i) }
$pool = [runspacefactory]::CreateRunspacePool(1, $Jobs)
$pool.Open()
$running = @()
$artifactsFull = (Resolve-Path -LiteralPath $ArtifactsRoot).Path
foreach ($chain in $chains) {
  $ps = [powershell]::Create()
  $ps.RunspacePool = $pool
  [void]$ps.AddScript($chainScript).AddArgument(@($chain)).AddArgument($runner).AddArgument($scriptRoot).AddArgument($artifactsFull).AddArgument($sandboxRoot).AddArgument($slots).AddArgument([bool]$Audible).AddArgument($root)
  $running += [pscustomobject]@{ ps = $ps; handle = $ps.BeginInvoke(); done = $false }
}
$results = @()
$stop = $false
while (@($running | Where-Object { -not $_.done }).Count -gt 0) {
  foreach ($r in $running) {
    if ($r.done -or -not $r.handle.IsCompleted) { continue }
    $r.done = $true
    $out = $r.ps.EndInvoke($r.handle)
    foreach ($err in $r.ps.Streams.Error) { Write-Host "    runner error: $err" }
    foreach ($res in @($out | Where-Object { $_.__case })) {
      $status = if ($res.exitCode -eq 0) { "ok" } else { "FAILED (exit $($res.exitCode)) - see $($res.artifacts)" }
      Write-Host ("==> {0,-34} {1,6}s  {2}" -f $res.name, $res.durationSeconds, $status)
      $results += $res
      if ($res.exitCode -ne 0 -and $StopOnFailure) { $stop = $true }
    }
    $r.ps.Dispose()
  }
  if ($stop) {
    foreach ($r in $running) { if (-not $r.done) { $r.ps.Stop(); $r.done = $true } }
    break
  }
  Start-Sleep -Milliseconds 200
}
$pool.Close()

# The exact sweep: every case's end state against tests/golden/sweep
$statesExit = 0
if (@($results | Where-Object { $_.name -like "sweep-*" -and $_.name -ne "sweep-playback" }).Count -gt 0) {
  Write-Host "==> exact sweep states"
  $statesArgs = @((Join-Path $PSScriptRoot "sweep\states.py"), $ArtifactsRoot)
  if ($UpdateGoldens) { $statesArgs += "--update" }
  & python @statesArgs | Select-Object -Last 40 | ForEach-Object { Write-Host "    $_" }
  $statesExit = $LASTEXITCODE
}

$ended = Get-Date
$summary = [pscustomobject]@{
  tier = $Tier
  jobs = $Jobs
  startedAt = $started.ToString("o")
  endedAt = $ended.ToString("o")
  durationSeconds = [math]::Round(($ended - $started).TotalSeconds, 2)
  passed = @($results | Where-Object { $_.exitCode -eq 0 }).Count
  failed = @($results | Where-Object { $_.exitCode -ne 0 }).Count
  goldenAudio = if ($NoAudio -or $Only) { "skipped" } elseif ($audioExit -eq 0) { "ok" } else { "FAILED" }
  exactSweep = if ($statesExit -eq 0) { "ok" } else { "FAILED" }
  results = @($results | Select-Object name, script, exitCode, memoryPeakKB, hostPeakKB, slot, startedAt, durationSeconds, artifacts)
}

$heaviest = $results | Where-Object { $_.memoryPeakKB } | Sort-Object memoryPeakKB -Descending | Select-Object -First 5
if ($heaviest) {
  Write-Host "Heaviest cases (device RSS KB): $(($heaviest | ForEach-Object { "$($_.name) $($_.memoryPeakKB)" }) -join ', ')"
}
$hostMax = ($results | Where-Object { $_.hostPeakKB } | Measure-Object -Property hostPeakKB -Maximum).Maximum
if ($hostMax) { Write-Host "Most Windows memory one simulator took: $([math]::Round($hostMax / 1024)) MB" }

$summaryPath = Join-Path $ArtifactsRoot "suite-summary.json"
$summary | ConvertTo-Json -Depth 5 | Out-File -LiteralPath $summaryPath -Encoding utf8

if ($summary.failed -gt 0 -or $audioExit -ne 0 -or $statesExit -ne 0) {
  Write-Error "RG Nano simulator suite ($Tier) failed: $($summary.failed) case(s), golden audio $($summary.goldenAudio), exact sweep $($summary.exactSweep). See $summaryPath"
  exit 1
}

Write-Host "RG Nano simulator suite ($Tier) passed: $($summary.passed) cases in $($summary.durationSeconds)s ($Jobs at once)"
Write-Host "Summary: $summaryPath"
exit 0
