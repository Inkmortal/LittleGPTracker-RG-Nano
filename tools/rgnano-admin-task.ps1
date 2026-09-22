# The one job that needs administrator rights when debugging on the RG Nano:
# share the USB device for passthrough and put the host end of the Nano's USB
# network on 192.168.137.1 so SSH to 192.168.137.2 works.
#
# Registered once as a scheduled task by tools/rgnano-admin-setup.ps1; after
# that it can be started without an admin prompt:
#     schtasks /run /tn RGNanoDebug
#
# It does these two things only - it is not a general "run as admin" hook.

$ErrorActionPreference = "Continue"
$log = Join-Path $env:TEMP "rgnano-admin-task.log"
"--- $(Get-Date -Format o) ---" | Out-File $log -Append

$usbipd = "C:\Program Files\usbipd-win\usbipd.exe"
if (Test-Path $usbipd) {
  $nano = & $usbipd list 2>&1 | Select-String "1d6b:0104"
  if ($nano -and $nano -notmatch "Shared") {
    $busid = ($nano -split "\s+")[0]
    & $usbipd bind --busid $busid 2>&1 | Out-File $log -Append
    "bound $busid" | Out-File $log -Append
  }
}

$adapter = Get-NetAdapter -ErrorAction SilentlyContinue |
  Where-Object { $_.InterfaceDescription -match "RNDIS|Remote NDIS" } | Select-Object -First 1
if ($adapter) {
  $have = Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -eq "192.168.137.1" }
  if (-not $have) {
    Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
      Where-Object { $_.PrefixOrigin -ne "WellKnown" } |
      Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue
    New-NetIPAddress -InterfaceIndex $adapter.ifIndex -IPAddress 192.168.137.1 -PrefixLength 24 -ErrorAction SilentlyContinue |
      Out-File $log -Append
  }
  "adapter $($adapter.Name): $((Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 | Select-Object -Expand IPAddress) -join ',')" | Out-File $log -Append
} else {
  "no RNDIS adapter present" | Out-File $log -Append
}
