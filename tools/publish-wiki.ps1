param(
  # gh account whose token pushes the wiki (leave empty to use git's own credentials)
  [string]$GitHubUser = "",
  [string]$Repo = "Inkmortal/LittleGPTracker-RG-Nano",
  [string]$Message = "Update wiki from docs/rgnano-wiki"
)

# Publishes docs/rgnano-wiki (pages + images) to the repository's GitHub wiki.
# The source of truth stays in the main repository; this script only mirrors it.

# Native git writes progress to stderr; don't treat that as a failure
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "docs\rgnano-wiki"
$work = Join-Path ([System.IO.Path]::GetTempPath()) "rgnano-wiki-publish"

$remote = "https://github.com/$Repo.wiki.git"
$gitAuth = @()
if ($GitHubUser) {
  $token = (gh auth token -u $GitHubUser).Trim()
  if (-not $token) { throw "No gh token for $GitHubUser" }
  $basic = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("x-access-token:$token"))
  $gitAuth = @("-c", "credential.helper=", "-c", "http.extraheader=Authorization: Basic $basic")
}

if (Test-Path $work) { Remove-Item -LiteralPath $work -Recurse -Force }
& git @gitAuth clone --quiet $remote $work 2>$null
if ($LASTEXITCODE -ne 0) {
  throw "Could not clone $remote. GitHub only creates a wiki repository after its first page is saved on github.com/$Repo/wiki - create any page there once, then rerun."
}

Get-ChildItem -LiteralPath $work -Force | Where-Object { $_.Name -ne ".git" } | Remove-Item -Recurse -Force
Copy-Item -Path (Join-Path $source "*") -Destination $work -Recurse -Force

& git -C $work add -A
& git -C $work -c user.name="$(git -C $root config user.name)" -c user.email="$(git -C $root config user.email)" commit --quiet -m $Message
if ($LASTEXITCODE -ne 0) { Write-Host "Wiki already up to date"; exit 0 }
& git -C $work @gitAuth push --quiet origin HEAD:master
if ($LASTEXITCODE -ne 0) { throw "Wiki push failed" }
Write-Host "Published wiki: https://github.com/$Repo/wiki"
