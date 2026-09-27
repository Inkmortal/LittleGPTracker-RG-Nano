# GBA launchers for the RG Nano (DrUm78 FunKey-OS): "GBA" runs mGBA (best for
# ROM hacks), "GBA gpSP" keeps the firmware's faster gpSP for heavy games.
# Both read /mnt/Game Boy Advance. The firmware's original launcher is kept
# here as gba_picoarch_funkey-s.original.opk.
param([string]$Card = "D:")
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$libretro = Join-Path $Card "Libretro"
if (-not (Test-Path (Join-Path $libretro "cores\mgba_libretro.so"))) { throw "no mGBA core at $libretro\cores" }
$wslHere = "/mnt/" + $here.Substring(0, 1).ToLower() + ($here.Substring(2) -replace '\\', '/')
foreach ($pair in @(@("mgba","gba_picoarch_funkey-s.opk"), @("gpsp","gba_gpsp_funkey-s.opk"))) {
  $src = $pair[0]; $opk = $pair[1]
  wsl -e bash -lc "set -e; t=`$(mktemp -d); cp '$wslHere/$src'/* `$t/; chmod 755 `$t/*.sh 2>/dev/null || true; mksquashfs `$t '/tmp/$opk' -all-root -noappend -no-exports -no-xattrs >/dev/null; rm -rf `$t"
  if ($LASTEXITCODE -ne 0) { throw "mksquashfs $opk failed" }
  $tmp = (wsl -e wslpath -w "/tmp/$opk").Trim()
  Copy-Item -LiteralPath $tmp -Destination (Join-Path $libretro $opk) -Force
  Write-Host "installed $opk"
}
