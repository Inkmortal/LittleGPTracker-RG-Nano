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

Write-Host "Packaging OPK..."
Copy-Item -LiteralPath (Join-Path $projects "lgpt-rgnano.elf") -Destination (Join-Path $projects "opk_build\lgpt-rgnano.elf") -Force
$opkOut = Join-Path $projects "lgpt-rgnano.opk"
wsl -e bash -lc "cd '$wslProjects' && mksquashfs opk_build lgpt-rgnano.opk -all-root -noappend -no-exports -no-xattrs >/dev/null"
if ($LASTEXITCODE -ne 0) { throw "mksquashfs failed" }

Write-Host "Installing..."
Copy-Item -LiteralPath $opkOut -Destination (Join-Path $nativeGames "lgpt-rgnano.opk") -Force
Copy-Item -LiteralPath (Join-Path $projects "resources\RGNANO\lgpt.png") -Destination (Join-Path $nativeGames "lgpt-rgnano.png") -Force
foreach ($doc in @("RGNANO_USER_MANUAL.md", "RGNANO_INPUT_MAP.md", "TRACKER_BASICS.md")) {
  Copy-Item -LiteralPath (Join-Path $root "docs\$doc") -Destination (Join-Path $applications $doc) -Force
}
New-Item -ItemType Directory -Force -Path $tracks | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $applications "Samples") | Out-Null
Get-ChildItem -LiteralPath (Join-Path $projects "resources\demos") -Directory | ForEach-Object {
  $dest = Join-Path $tracks $_.Name
  if ((Test-Path $dest) -and -not $UpdateDemos) {
    Write-Host "  keeping existing $($_.Name) (use -UpdateDemos to replace)"
  } else {
    if (Test-Path $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Copy-Item -LiteralPath $_.FullName -Destination $dest -Recurse
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
