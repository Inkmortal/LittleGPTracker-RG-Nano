param(
  # Drive letter of the RG Nano SD card; found automatically when omitted
  [string]$Drive = "",
  # Skip the ARM build and install the existing projects/lgpt-rgnano.elf
  [switch]$NoBuild,
  # Replace demo song folders that already exist on the card
  [switch]$UpdateDemos
)

# Builds the RG Nano binary (FunKey SDK inside WSL), packages the OPK, and
# installs it plus docs and demo songs onto the RG Nano SD card.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$projects = Join-Path $root "projects"

function Convert-ToWslPath([string]$path) {
  $full = (Resolve-Path -LiteralPath $path).Path
  $drive = $full.Substring(0, 1).ToLower()
  return "/mnt/$drive" + ($full.Substring(2) -replace '\\', '/')
}

if (-not $Drive) {
  $card = Get-PSDrive -PSProvider FileSystem | Where-Object {
    (Test-Path (Join-Path $_.Root "Native games")) -and (Test-Path (Join-Path $_.Root "Applications"))
  } | Select-Object -First 1
  if (-not $card) {
    throw "RG Nano SD card not found. Connect the RG Nano over USB (or insert its SD card) and retry."
  }
  $Drive = $card.Name
}
$cardRoot = "$($Drive):\"
$nativeGames = Join-Path $cardRoot "Native games"
$applications = Join-Path $cardRoot "Applications"
$tracks = Join-Path $applications "Tracks"
Write-Host "RG Nano card: $cardRoot"

$wslProjects = Convert-ToWslPath $projects
if (-not $NoBuild) {
  Write-Host "Building lgpt-rgnano.elf (WSL + FunKey SDK)..."
  wsl -e bash -lc "cd '$wslProjects' && make PLATFORM=RGNANO -j4 2>&1 | grep -E ' error|Error [0-9]' ; test -f lgpt-rgnano.elf"
  if ($LASTEXITCODE -ne 0) { throw "RG Nano build failed" }
}

# Run the device's own code under qemu-arm: the Windows simulator can't catch
# device-only bugs like the Nano's broken libm exp()
$wslRoot = Convert-ToWslPath $root
foreach ($check in @("math_check.cpp", "synth_release_check.cpp", "mod_check.cpp", "chorus_check.cpp", "crash_check.cpp", "eq_check.cpp", "synth_engines_check.cpp")) {
  Write-Host "ARM check: $check"
  wsl -e bash -lc "set -o pipefail; cd '$wslRoot' && bash tools/dsp-harness/run.sh tools/dsp-harness/$check 2>&1 | grep -v '^\['"
  if ($LASTEXITCODE -ne 0) { throw "ARM check failed: $check" }
}

Write-Host "Packaging OPK..."
# Built-in guide from the wiki, shipped next to the binary (bin:guide.txt)
python (Join-Path $PSScriptRoot "build_ingame_guide.py") | Out-Host
if ($LASTEXITCODE -ne 0) { throw "guide build failed" }
Copy-Item -LiteralPath (Join-Path $projects "resources\guide\guide.txt") -Destination (Join-Path $projects "opk_build\guide.txt") -Force
# The app logs this on every launch, so the build that ran can be identified
$buildId = (git -C $root rev-parse --short HEAD).Trim()
if ((git -C $root status --porcelain -- sources projects).Length -gt 0) { $buildId = "$buildId+local" }
Set-Content -LiteralPath (Join-Path $projects "opk_build\build-id.txt") -Value $buildId -NoNewline -Encoding ascii
Copy-Item -LiteralPath (Join-Path $projects "lgpt-rgnano.elf") -Destination (Join-Path $projects "opk_build\lgpt-rgnano.elf") -Force
# Keep this build's symbols: a crash report from the device names its commit
$elfArchive = Join-Path $root "build\elf"
New-Item -ItemType Directory -Force -Path $elfArchive | Out-Null
# (the unstripped copy the Makefile keeps as lgpt-rgnano.elf.debug)
Copy-Item -LiteralPath (Join-Path $projects "lgpt-rgnano.elf.debug") -Destination (Join-Path $elfArchive "$buildId.elf") -Force
$opkOut = Join-Path $projects "lgpt-rgnano.opk"
wsl -e bash -lc "cd '$wslProjects' && mksquashfs opk_build lgpt-rgnano.opk -all-root -noappend -no-exports -no-xattrs >/dev/null"
if ($LASTEXITCODE -ne 0) { throw "mksquashfs failed" }

Write-Host "Installing..."
Copy-Item -LiteralPath $opkOut -Destination (Join-Path $nativeGames "lgpt-rgnano.opk") -Force
Copy-Item -LiteralPath (Join-Path $projects "resources\RGNANO\lgpt.png") -Destination (Join-Path $nativeGames "lgpt-rgnano.png") -Force
# The guide: the same pages as the website and the in-app Help, readable on
# a computer from the card. The old separate manuals are replaced by it.
foreach ($old in @("RGNANO_USER_MANUAL.md", "RGNANO_INPUT_MAP.md", "TRACKER_BASICS.md")) {
  $stale = Join-Path $applications $old
  if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale -Force }
}
$guideDest = Join-Path $applications "LGPT-Guide"
New-Item -ItemType Directory -Force -Path (Join-Path $guideDest "images") | Out-Null
$wiki = Join-Path (Join-Path $root "docs") "rgnano-wiki"
Get-ChildItem -LiteralPath $wiki -Filter *.md |
  Where-Object { -not $_.Name.StartsWith("_") } | Copy-Item -Destination $guideDest -Force
Copy-Item -Path (Join-Path (Join-Path $wiki "images") "*") -Destination (Join-Path $guideDest "images") -Force
New-Item -ItemType Directory -Force -Path $tracks | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $applications "Samples") | Out-Null
# Sample packs: our files are refreshed, anything you added to a pack folder stays
Get-ChildItem -LiteralPath (Join-Path $projects "resources\samples") -Directory | ForEach-Object {
  $packDest = Join-Path $applications "Samples\$($_.Name)"
  New-Item -ItemType Directory -Force -Path $packDest | Out-Null
  Get-ChildItem -LiteralPath $_.FullName -File | Where-Object { $_.Extension -in ".wav", ".md" } |
    Copy-Item -Destination $packDest -Force
}
Write-Host "  sample packs: $((Get-ChildItem -LiteralPath (Join-Path $projects 'resources\samples') -Directory).Name -join ', ')"
# Demo songs: a demo you never edited is updated; one you saved over keeps
# your version. installed.hash remembers what we copied last time.
function Get-SongHash([string]$folder) {
  $song = Join-Path $folder "lgptsav.dat"
  if (-not (Test-Path -LiteralPath $song)) { return "" }
  return (Get-FileHash -LiteralPath $song -Algorithm SHA256).Hash
}
Get-ChildItem -LiteralPath (Join-Path $projects "resources\demos") -Directory | ForEach-Object {
  $dest = Join-Path $tracks $_.Name
  $marker = Join-Path $dest ".installed.hash"
  $replace = $true
  if ((Test-Path $dest) -and -not $UpdateDemos) {
    $installed = if (Test-Path -LiteralPath $marker) { (Get-Content -LiteralPath $marker -Raw).Trim() } else { "" }
    $current = Get-SongHash $dest
    if ($current -eq (Get-SongHash $_.FullName)) {
      $replace = $true   # already this version; refresh samples and marker
    } elseif ($installed -and $installed -eq $current) {
      $replace = $true   # untouched since the last install
    } elseif ($installed) {
      $replace = $false  # the user saved changes into it
      Write-Host "  keeping $($_.Name): it has your edits (use -UpdateDemos to replace)"
    } else {
      # Installed before hashes were recorded: keep a copy, then update
      $backup = "$dest-backup-$(Get-Date -Format yyyyMMdd-HHmmss)"
      Copy-Item -LiteralPath $dest -Destination $backup -Recurse
      Write-Host "  $($_.Name): previous copy kept as $(Split-Path -Leaf $backup)"
    }
  }
  if ($replace) {
    if (Test-Path $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Copy-Item -LiteralPath $_.FullName -Destination $dest -Recurse
    Set-Content -LiteralPath $marker -Value (Get-SongHash $_.FullName) -NoNewline
    Write-Host "  demo $($_.Name)"
  }
}

$commit = (git -C $root rev-parse --short HEAD).Trim()
$dirty = if ((git -C $root status --porcelain -- sources projects/Makefile projects/resources).Length -gt 0) { " (+uncommitted)" } else { "" }
$stamp = Get-Date -Format "o"
@(
  "commit $commit$dirty",
  "branch $((git -C $root rev-parse --abbrev-ref HEAD).Trim())",
  "installed $stamp",
  "OPK=$nativeGames\lgpt-rgnano.opk",
  "LOG=/mnt/Applications/lgpt-rgnano.log"
) | Set-Content -LiteralPath (Join-Path $nativeGames "LGPT-RGNANO-BUILD-MARKER.txt") -Encoding ascii
"COMMIT=$commit$dirty`nINSTALLED=$stamp`nINSTALL=Native games" | Set-Content -LiteralPath (Join-Path $applications "lgpt-rgnano-build.txt") -Encoding ascii

Write-Host "Installed commit $commit$dirty on $cardRoot. Eject the card before unplugging."
