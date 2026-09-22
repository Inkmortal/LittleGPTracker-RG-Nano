param(
  [ValidateSet("on", "off", "status", "connect")]
  [string]$Action = "status",
  [string]$DeviceIp = "192.168.137.2",
  [string]$HostIp = "192.168.137.1"
)

# USB networking + SSH on the RG Nano, for debugging on the real device.
#
# The firmware (FunKey-OS, DrUm78 fork) checks the FAT partition at boot:
# a file named "usbnet" makes it bring up an RNDIS USB network interface
# (device 192.168.137.2/24) and start the dropbear SSH server, while still
# exposing the SD card as a USB drive. No file = normal behaviour.
#
#   .\tools\rgnano-usbnet.ps1 on       # create the flag (then reboot the Nano)
#   .\tools\rgnano-usbnet.ps1 status   # flag present? adapter up? SSH reachable?
#   .\tools\rgnano-usbnet.ps1 connect  # set the host adapter IP (needs admin)
#   .\tools\rgnano-usbnet.ps1 off      # remove the flag (back to plain USB drive)
#
# Login: root / funkey (stock FunKey OS password).

$ErrorActionPreference = "Stop"

function Get-CardRoot {
  foreach ($drive in Get-Volume | Where-Object { $_.DriveType -eq "Removable" -and $_.DriveLetter }) {
    $root = "$($drive.DriveLetter):\"
    if (Test-Path (Join-Path $root "Native games")) { return $root }
  }
  return $null
}

function Get-UsbAdapter {
  Get-NetAdapter -ErrorAction SilentlyContinue |
    Where-Object { $_.InterfaceDescription -match "RNDIS|Remote NDIS|USB Ethernet|FunKey" } |
    Select-Object -First 1
}

$card = Get-CardRoot
switch ($Action) {
  "on" {
    if (-not $card) { throw "RG Nano SD card not found (plug it in as a USB drive first)" }
    $flag = Join-Path $card "usbnet"
    if (-not (Test-Path $flag)) { New-Item -ItemType File -Path $flag | Out-Null }
    Write-Host "Flag written: $flag"
    Write-Host "Now power the Nano off and on with the cable connected."
  }
  "off" {
    if (-not $card) { throw "RG Nano SD card not found" }
    $flag = Join-Path $card "usbnet"
    if (Test-Path $flag) { Remove-Item -LiteralPath $flag -Force }
    Write-Host "Flag removed: USB networking and SSH are off after the next reboot."
  }
  "connect" {
    $adapter = Get-UsbAdapter
    if (-not $adapter) { throw "No USB network adapter found; is the Nano booted with the flag?" }
    $existing = Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
      Where-Object { $_.IPAddress -eq $HostIp }
    if (-not $existing) {
      New-NetIPAddress -InterfaceIndex $adapter.ifIndex -IPAddress $HostIp -PrefixLength 24 | Out-Null
    }
    Write-Host "Host $HostIp on '$($adapter.Name)'. Device should answer at $DeviceIp."
    Test-NetConnection -ComputerName $DeviceIp -Port 22 -InformationLevel Quiet
  }
  default {
    Write-Host "card:    $(if ($card) { $card } else { 'not mounted' })"
    if ($card) {
      Write-Host "flag:    $(if (Test-Path (Join-Path $card 'usbnet')) { 'usbnet present' } else { 'absent' })"
    }
    $adapter = Get-UsbAdapter
    Write-Host "adapter: $(if ($adapter) { "$($adapter.Name) ($($adapter.Status))" } else { 'none' })"
    if ($adapter) {
      $ssh = Test-NetConnection -ComputerName $DeviceIp -Port 22 -InformationLevel Quiet -WarningAction SilentlyContinue
      Write-Host "ssh:     $(if ($ssh) { "open on $DeviceIp" } else { 'no answer' })"
    }
  }
}
