<#
.SYNOPSIS
  Minimal TX Inhibit KEY-agent stand-in for Windows. Needs no keyboard focus.

.DESCRIPTION
  Sends NetworkMessage::TxInhibit (type 18) UDP to the dedicated inhibit port
  (docs/TX_INHIBIT.md) so TX Inhibit can be exercised while you drive WSJT-X.

  The interactive helper (inhibit-test) reads a held key and therefore
  needs focus itself, which makes it impossible to hold the KEY and
  press Tune in WSJT-X at the same time. This script holds from a
  separate window instead, leaving WSJT-X free.

  Mirrors the Hold-sender half of a real KEY agent (docs/TX_INHIBIT.md §3):
  an immediate hold, keepalives about every 200 ms, and an explicit release
  (TTL 0) on exit for this Controller ID only.

  Fail-safe: if this script is killed without releasing, the station's own
  lease timeout clears this controller's row after -TtlMs (default 600 ms).

.EXAMPLE
  .\Send-InhibitHold.ps1

.EXAMPLE
  .\Send-InhibitHold.ps1 -Seconds 20

.EXAMPLE
  .\Send-InhibitHold.ps1 -Release

.NOTES
  -TargetHost, not -Host: $Host is a reserved PowerShell automatic variable.
#>
[CmdletBinding()]
param(
  [string]$TargetHost = '127.0.0.1',
  [int]$Port = 22372,
  [string]$ControllerId = 'PS-TEST',
  [string]$Station = '',
  # 0 = hold until Ctrl+C
  [int]$Seconds = 0,
  # Wire TTL ms: how long the station keeps this lease without a new packet.
  # Protocol range is 100..30000 (0 = release this controller).
  [int]$TtlMs = 600,
  [int]$KeepaliveMs = 200,
  [switch]$Release
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrEmpty($ControllerId)) {
  throw 'ControllerId must be non-empty (lease key)'
}
if ($Station -eq '') {
  $Station = $ControllerId
}
if ($TtlMs -ne 0 -and ($TtlMs -lt 100 -or $TtlMs -gt 30000)) {
  throw "TtlMs must be 0 or 100..30000 (protocol range); got $TtlMs"
}

function Encode-QByteArray([byte[]]$Data) {
  $len = [BitConverter]::GetBytes([uint32]$Data.Length)
  if ([BitConverter]::IsLittleEndian) { [Array]::Reverse($len) }
  return $len + $Data
}

function Encode-TxInhibit([string]$Controller, [uint32]$Ttl, [string]$StationText) {
  # magic, schema 3, type 18 — big-endian quint32
  $hdr = New-Object byte[] 12
  $vals = @([uint32]0xADBCCBDA, [uint32]3, [uint32]18)
  for ($i = 0; $i -lt 3; $i++) {
    $b = [BitConverter]::GetBytes($vals[$i])
    if ([BitConverter]::IsLittleEndian) { [Array]::Reverse($b) }
    [Array]::Copy($b, 0, $hdr, $i * 4, 4)
  }
  $utf8 = [Text.Encoding]::UTF8
  $targetId = Encode-QByteArray @()   # empty Id (ignored on 22372)
  $controller = Encode-QByteArray ($utf8.GetBytes($Controller))
  $ttlBytes = [BitConverter]::GetBytes($Ttl)
  if ([BitConverter]::IsLittleEndian) { [Array]::Reverse($ttlBytes) }
  $station = Encode-QByteArray ($utf8.GetBytes($StationText))
  return $hdr + $targetId + $controller + $ttlBytes + $station
}

$udp = [System.Net.Sockets.UdpClient]::new()

function Send-Hold {
  param([int]$Ttl)
  $bytes = Encode-TxInhibit -Controller $ControllerId -Ttl ([uint32]$Ttl) -StationText $Station
  [void]$udp.Send($bytes, $bytes.Length, $TargetHost, $Port)
  return $bytes.Length
}

try {
  if ($Release) {
    $n = Send-Hold -Ttl 0
    Write-Host "release controller=$ControllerId -> ${TargetHost}:${Port}  ($n bytes type-18)"
    return
  }

  Write-Host "HOLD controller=$ControllerId station=$Station ttl_ms=$TtlMs -> ${TargetHost}:${Port}"
  [void](Send-Hold -Ttl $TtlMs)
  $deadline = if ($Seconds -gt 0) { [datetime]::UtcNow.AddSeconds($Seconds) } else { [datetime]::MaxValue }
  while ([datetime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds $KeepaliveMs
    [void](Send-Hold -Ttl $TtlMs)
  }
}
finally {
  if (-not $Release) {
    try {
      [void](Send-Hold -Ttl 0)
      Write-Host "RELEASE controller=$ControllerId -> ${TargetHost}:${Port}"
    } catch {}
  }
  $udp.Dispose()
}
