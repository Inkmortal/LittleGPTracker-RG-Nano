param(
  # Only this kit (the script's name without .rgsim), e.g. -Kit Chinese
  [string]$Kit = ""
)

# Builds the kits that ship with the app: each tools/kits/<Name>.rgsim sets
# up the song's sounds in the simulator and saves them with
# "sim_save_kit <Name>"; the kit file it writes is copied to
# projects/resources/sounds/kits, which install-rgnano.ps1 puts in
# Applications/Sounds/kits on the card. Build the sim first
# (tools/build-rgnano-sim.ps1).

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "projects\resources\sounds\kits"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$scripts = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "kits") -Filter "*.rgsim" -File
if ($Kit) { $scripts = $scripts | Where-Object { $_.BaseName -eq $Kit } }
if (-not $scripts) { throw "no kit script found" }

Push-Location $root
try {
  foreach ($script in $scripts) {
    $name = $script.BaseName
    & powershell -NoProfile -File (Join-Path $PSScriptRoot "run-rgnano-sim.ps1") -Script $script.FullName -Mute -ResetLastProject
    if ($LASTEXITCODE -ne 0) { throw "kit ${name}: the sim run failed (see projects\rgnano-sim.log)" }
    $made = Join-Path $root "rgnano-sim-data\sounds\kits\$name.lgk"
    if (-not (Test-Path -LiteralPath $made)) { throw "kit ${name}: $made was not written" }
    Copy-Item -LiteralPath $made -Destination (Join-Path $out "$name.lgk") -Force
    Write-Host "kit $name -> projects\resources\sounds\kits\$name.lgk"
  }
} finally {
  Pop-Location
}
