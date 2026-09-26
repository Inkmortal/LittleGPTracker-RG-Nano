param(
  [string]$ExePath = ".\projects\lgpt-rgnano-sim.exe",
  [string]$Script = "",
  [switch]$Skin,
  [switch]$SeedSampleFixture,
  [switch]$SeedLofiFixture,
  # Copy the shipped sample packs (projects/resources/samples) into the sim
  [switch]$SeedSamplePacks,
  [switch]$ResetLastProject,
  # Copy a shipped demo song (e.g. JadeSword) into the sim and auto-load it
  [string]$OpenDemo = "",
  [switch]$Mute,
  # Seed for the suggested song names, so they are the same every run
  [string]$NameSeed = "",
  # No key auto-repeat: every scripted press is exactly one press
  [switch]$NoKeyRepeat,
  [switch]$Visible,
  [string]$ArtifactsDir = ""
)

$root = Split-Path -Parent $PSScriptRoot
$resourceDir = Join-Path $root "projects\resources\RGNANO_SIM"
$exeFullPath = Resolve-Path -LiteralPath $ExePath -ErrorAction SilentlyContinue
$routeHelpers = Join-Path $PSScriptRoot "rgnano-sim-routes.ps1"
. $routeHelpers

if (-not $exeFullPath) {
  throw "Simulator executable not found: $ExePath. Build it with: cd projects; make PLATFORM=RGNANO_SIM"
}

$exeFullPath = $exeFullPath.Path
$exeDir = Split-Path -Parent $exeFullPath
$runStarted = Get-Date

if ($ArtifactsDir) {
  New-Item -ItemType Directory -Force -Path $ArtifactsDir | Out-Null
  Get-ChildItem -LiteralPath $ArtifactsDir -File | Remove-Item -Force
}

Copy-Item -LiteralPath (Join-Path $resourceDir "config.xml") -Destination (Join-Path $exeDir "config.xml") -Force
Copy-Item -LiteralPath (Join-Path $resourceDir "mapping.xml") -Destination (Join-Path $exeDir "mapping.xml") -Force
# Built-in guide (tools/build_ingame_guide.py), read as bin:guide.txt
Copy-Item -LiteralPath (Join-Path $root "projects\resources\guide\guide.txt") -Destination (Join-Path $exeDir "guide.txt") -Force

$sdlDll = Join-Path $root "projects\resources\W32\SDL.dll"
if (Test-Path -LiteralPath $sdlDll) {
  Copy-Item -LiteralPath $sdlDll -Destination (Join-Path $exeDir "SDL.dll") -Force
}

$mingwBin = "C:\msys64\mingw32\bin"
foreach ($runtimeDll in @("libgcc_s_dw2-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
  $runtimePath = Join-Path $mingwBin $runtimeDll
  if (Test-Path -LiteralPath $runtimePath) {
    Copy-Item -LiteralPath $runtimePath -Destination (Join-Path $exeDir $runtimeDll) -Force
  }
}

$dataDir = Join-Path (Get-Location) "rgnano-sim-data"
New-Item -ItemType Directory -Force -Path (Join-Path $dataDir "tracks") | Out-Null
$sampleDir = Join-Path $dataDir "samples"
New-Item -ItemType Directory -Force -Path $sampleDir | Out-Null

function Write-TestWav {
  param([string]$Path)
  $sampleRate = 8000
  $durationSeconds = 1
  $sampleCount = $sampleRate * $durationSeconds
  $dataSize = $sampleCount * 2
  $writer = [System.IO.BinaryWriter]::new([System.IO.File]::Open($Path, [System.IO.FileMode]::Create))
  try {
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
    $writer.Write([int](36 + $dataSize))
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVEfmt "))
    $writer.Write([int]16)
    $writer.Write([Int16]1)
    $writer.Write([Int16]1)
    $writer.Write([int]$sampleRate)
    $writer.Write([int]($sampleRate * 2))
    $writer.Write([Int16]2)
    $writer.Write([Int16]16)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
    $writer.Write([int]$dataSize)
    for ($i = 0; $i -lt $sampleCount; $i++) {
      $sample = [Int16]([Math]::Sin((2 * [Math]::PI * 440 * $i) / $sampleRate) * 12000)
      $writer.Write($sample)
    }
  } finally {
    $writer.Dispose()
  }
}

function Write-LofiWav {
  param(
    [string]$Path,
    [string]$Kind
  )
  $sampleRate = 16000
  $durationSeconds = switch ($Kind) {
    "kick" { 0.45 }
    "snare" { 0.35 }
    "hat" { 0.18 }
    "chord" { 2.0 }
    "noise" { 4.0 }
    "guzheng" { 1.4 }
    "dizi" { 1.2 }
    "erhu" { 1.8 }
    "drum" { 0.7 }
    default { 1.0 }
  }
  $sampleCount = [int]($sampleRate * $durationSeconds)
  $dataSize = $sampleCount * 2
  $rand = [System.Random]::new(1337 + $Kind.GetHashCode())
  $writer = [System.IO.BinaryWriter]::new([System.IO.File]::Open($Path, [System.IO.FileMode]::Create))
  try {
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
    $writer.Write([int](36 + $dataSize))
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVEfmt "))
    $writer.Write([int]16)
    $writer.Write([Int16]1)
    $writer.Write([Int16]1)
    $writer.Write([int]$sampleRate)
    $writer.Write([int]($sampleRate * 2))
    $writer.Write([Int16]2)
    $writer.Write([Int16]16)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
    $writer.Write([int]$dataSize)

    for ($i = 0; $i -lt $sampleCount; $i++) {
      $t = $i / $sampleRate
      $env = [Math]::Exp(-7.0 * $t)
      $value = switch ($Kind) {
        "kick" {
          $freq = 52 + (80 * [Math]::Exp(-10.0 * $t))
          [Math]::Sin((2 * [Math]::PI * $freq * $t)) * 22000 * [Math]::Exp(-8.0 * $t)
        }
        "snare" {
          (($rand.NextDouble() * 2.0) - 1.0) * 13000 * [Math]::Exp(-11.0 * $t) +
            [Math]::Sin((2 * [Math]::PI * 190 * $t)) * 5000 * [Math]::Exp(-9.0 * $t)
        }
        "hat" {
          $noise = (($rand.NextDouble() * 2.0) - 1.0)
          $noise * 10000 * [Math]::Exp(-28.0 * $t)
        }
        "chord" {
          $freqs = @(220.0, 261.63, 329.63, 392.0)
          $sum = 0.0
          foreach ($freq in $freqs) {
            $sum += [Math]::Sin((2 * [Math]::PI * $freq * $t))
          }
          ($sum / $freqs.Count) * 9500 * (0.75 + 0.25 * [Math]::Sin(2 * [Math]::PI * 0.6 * $t))
        }
        "noise" {
          (($rand.NextDouble() * 2.0) - 1.0) * 900
        }
        "guzheng" {
          $base = 293.66
          $pluck = [Math]::Sin(2 * [Math]::PI * $base * $t) +
            0.45 * [Math]::Sin(2 * [Math]::PI * $base * 2.01 * $t) +
            0.22 * [Math]::Sin(2 * [Math]::PI * $base * 3.02 * $t)
          $pluck * 15000 * [Math]::Exp(-3.2 * $t)
        }
        "dizi" {
          $vibrato = 1.0 + 0.012 * [Math]::Sin(2 * [Math]::PI * 5.2 * $t)
          $base = 587.33 * $vibrato
          ($([Math]::Sin(2 * [Math]::PI * $base * $t)) +
            0.18 * [Math]::Sin(2 * [Math]::PI * $base * 2.0 * $t)) * 10000 *
            [Math]::Min(1.0, $t * 12.0) * [Math]::Exp(-0.8 * $t)
        }
        "erhu" {
          $vibrato = 1.0 + 0.018 * [Math]::Sin(2 * [Math]::PI * 4.8 * $t)
          $base = 220.0 * $vibrato
          $saw = 2.0 * (($base * $t) - [Math]::Floor(0.5 + $base * $t))
          $sine = [Math]::Sin(2 * [Math]::PI * $base * $t)
          (($saw * 0.45) + ($sine * 0.55)) * 9000 *
            [Math]::Min(1.0, $t * 7.0) * [Math]::Exp(-0.35 * $t)
        }
        "drum" {
          $thump = [Math]::Sin(2 * [Math]::PI * 92 * $t) * 16000 * [Math]::Exp(-7.0 * $t)
          $skin = (($rand.NextDouble() * 2.0) - 1.0) * 5000 * [Math]::Exp(-13.0 * $t)
          $thump + $skin
        }
        default {
          [Math]::Sin((2 * [Math]::PI * 440 * $t)) * 8000 * $env
        }
      }
      if ($value -gt 32767) { $value = 32767 }
      if ($value -lt -32768) { $value = -32768 }
      $writer.Write([Int16]$value)
    }
  } finally {
    $writer.Dispose()
  }
}

if ($SeedSampleFixture -or $SeedLofiFixture -or $SeedSamplePacks) {
  Get-ChildItem -LiteralPath $sampleDir -Filter "*.wav" -File -ErrorAction SilentlyContinue | Remove-Item -Force
  # Pack folders from a -SeedSamplePacks run would change what the sample
  # browser lists first
  Get-ChildItem -LiteralPath $sampleDir -Directory -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
}

if ($SeedSamplePacks) {
  # The sample packs the install puts in Applications/Samples
  Get-ChildItem -LiteralPath (Join-Path $root "projects\resources\samples") -Directory | ForEach-Object {
    $packDest = Join-Path $sampleDir $_.Name
    New-Item -ItemType Directory -Force -Path $packDest | Out-Null
    Get-ChildItem -LiteralPath $_.FullName -File | Where-Object { $_.Extension -in ".wav", ".md" } |
      Copy-Item -Destination $packDest -Force
  }
}

if ($SeedSampleFixture) {
  Write-TestWav -Path (Join-Path $sampleDir "rgnano-test-tone.wav")
}

if ($SeedLofiFixture) {
  Write-LofiWav -Path (Join-Path $sampleDir "lofi-kick.wav") -Kind "kick"
  Write-LofiWav -Path (Join-Path $sampleDir "lofi-snare.wav") -Kind "snare"
  Write-LofiWav -Path (Join-Path $sampleDir "lofi-hat.wav") -Kind "hat"
  Write-LofiWav -Path (Join-Path $sampleDir "lofi-chord.wav") -Kind "chord"
  Write-LofiWav -Path (Join-Path $sampleDir "lofi-vinyl.wav") -Kind "noise"
  Write-LofiWav -Path (Join-Path $sampleDir "wuxia-guzheng.wav") -Kind "guzheng"
  Write-LofiWav -Path (Join-Path $sampleDir "wuxia-dizi.wav") -Kind "dizi"
  Write-LofiWav -Path (Join-Path $sampleDir "wuxia-erhu.wav") -Kind "erhu"
  Write-LofiWav -Path (Join-Path $sampleDir "wuxia-drum.wav") -Kind "drum"
}

if ($ResetLastProject) {
  $lastProjectFile = Join-Path $exeDir "last_project"
  if (Test-Path -LiteralPath $lastProjectFile) {
    Remove-Item -LiteralPath $lastProjectFile -Force
  }
  # Song list order and recent list start fresh too
  foreach ($state in ".lgpt-sort", ".lgpt-recent") {
    $stateFile = Join-Path $dataDir "tracks\$state"
    if (Test-Path -LiteralPath $stateFile) {
      Remove-Item -LiteralPath $stateFile -Force
    }
  }
}

if ($OpenDemo) {
  $demoSource = Join-Path $root "projects\resources\demos\lgpt_$OpenDemo"
  if (-not (Test-Path -LiteralPath $demoSource)) {
    # Songs kept only for tests (e.g. saved in an old format)
    $demoSource = Join-Path $root "projects\resources\RGNANO_SIM\fixtures\lgpt_$OpenDemo"
  }
  if (-not (Test-Path -LiteralPath $demoSource)) {
    throw "Demo song not found: $OpenDemo (projects/resources/demos or RGNANO_SIM/fixtures)"
  }
  $demoTarget = Join-Path $dataDir "tracks\lgpt_$OpenDemo"
  if (Test-Path -LiteralPath $demoTarget) {
    Remove-Item -LiteralPath $demoTarget -Recurse -Force
  }
  Copy-Item -LiteralPath $demoSource -Destination $demoTarget -Recurse
  Set-Content -LiteralPath (Join-Path $exeDir "last_project") -Value "./rgnano-sim-data/tracks/lgpt_$OpenDemo" -NoNewline -Encoding ascii
}

$args = @()
if ($Script) {
  $scriptToRun = $Script
  $scriptText = Get-Content -LiteralPath $Script -Raw
  if ($scriptText -match '(?m)^\s*route\s+') {
    $expandedScript = Join-Path $exeDir "rgnano-sim-expanded.rgsim"
    Expand-RGNanoSimScript -ScriptPath $Script -OutputPath $expandedScript
    if (-not (Test-Path -LiteralPath $expandedScript)) {
      throw "Failed to expand RG Nano simulator script: $Script"
    }
    $scriptToRun = $expandedScript
  }
  $args += "-RGNANOSIM_SCRIPT=$scriptToRun"
}
$args += "-RGNANOSIM_LOG=$(Join-Path $exeDir 'rgnano-sim.log')"
if ($Mute) {
  # Audio is still rendered, measured and captured, just not played
  $args += "-RGNANOSIM_MUTE=YES"
}
if ($NoKeyRepeat) {
  # Long scripts of single presses: a busy machine can hold a key past the
  # repeat delay (250 ms) and turn one press into two
  $args += "-KEYDELAY=600000"
}
if ($NameSeed) {
  # The same "random" song names every run (walkthrough screenshots)
  $args += "-RGNANOSIM_NAMESEED=$NameSeed"
}
if ($Skin -or -not $Script) {
  $args += "-RGNANOSIM_SKIN=YES"
}

# Scripted runs are headless: the simulator draws into a window that is never
# shown, so nothing pops up or steals focus. Pass -Visible to watch.
if ($Script -and -not $Visible) {
  $args += "-RGNANOSIM_HEADLESS=YES"
}

# The simulator is a GUI-subsystem app (no console window), so wait on the
# process explicitly to get its exit code.
$quotedArgs = @($args | ForEach-Object { '"' + $_ + '"' })
if ($quotedArgs.Count -gt 0) {
  $process = Start-Process -FilePath $exeFullPath -ArgumentList $quotedArgs -Wait -PassThru -NoNewWindow
} else {
  $process = Start-Process -FilePath $exeFullPath -Wait -PassThru -NoNewWindow
}

$exitCode = $process.ExitCode

if ($ArtifactsDir) {
  $logPath = Join-Path $exeDir "rgnano-sim.log"
  if (Test-Path -LiteralPath $logPath) {
    Copy-Item -LiteralPath $logPath -Destination (Join-Path $ArtifactsDir "rgnano-sim.log") -Force
  }
  Get-ChildItem -Path (Get-Location) -Filter "*.bmp" -File | Where-Object { $_.LastWriteTime -ge $runStarted } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $ArtifactsDir $_.Name) -Force
  }
  Get-ChildItem -Path (Get-Location) -Filter "*.wav" -File | Where-Object { $_.LastWriteTime -ge $runStarted } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $ArtifactsDir $_.Name) -Force
  }
}

exit $exitCode
