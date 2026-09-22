# Run once, with administrator rights (one UAC prompt):
#
#     powershell -ExecutionPolicy Bypass -File tools\rgnano-admin-setup.ps1
#
# Registers the scheduled task "RGNanoDebug", which runs
# tools/rgnano-admin-task.ps1 elevated. After this, the USB network to the
# RG Nano can be brought up without any further prompt:
#
#     schtasks /run /tn RGNanoDebug
#
# Remove it again with:  schtasks /delete /tn RGNanoDebug /f

$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
      [Security.Principal.WindowsBuiltInRole]::Administrator)) {
  Write-Host "Not elevated: asking for administrator rights..."
  Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath
  return
}

$task = Join-Path $PSScriptRoot "rgnano-admin-task.ps1"
$action = New-ScheduledTaskAction -Execute "powershell.exe" `
  -Argument "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$task`""
$principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
  -ExecutionTimeLimit (New-TimeSpan -Minutes 5)
Register-ScheduledTask -TaskName "RGNanoDebug" -Action $action -Principal $principal -Settings $settings -Force | Out-Null
Write-Host "Registered scheduled task 'RGNanoDebug'."
Write-Host "Bring the Nano's USB network up any time with: schtasks /run /tn RGNanoDebug"
