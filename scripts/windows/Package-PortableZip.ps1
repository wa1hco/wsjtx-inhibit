#Requires -Version 5.1
<#
.SYNOPSIS
  Package stage tree as a portable ZIP for GitHub Releases / seat drop.
#>
param(
  [string]$StageDir = "C:\src\wsjtx-prefix\stage",
  [string]$OutDir = "C:\src\wsjtx-prefix\dist",
  [string]$Version = "3.0.2-wims-dev",
  [string]$Product = "wsjtx-inhibit"
)

$ErrorActionPreference = "Stop"
$wsjtx = Join-Path $StageDir "bin\wsjtx.exe"
if (-not (Test-Path $wsjtx)) { throw "Missing $wsjtx" }

$name = "$Product-$Version-windows-x86_64"
$staging = Join-Path $OutDir $name
$zip = Join-Path $OutDir "$name.zip"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null
robocopy $StageDir $staging /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed: $LASTEXITCODE" }

# Bundle hamlib runtime if not already in stage
$destDll = Join-Path $staging "bin\libhamlib-4.dll"
if (-not (Test-Path $destDll)) {
  $srcDll = "C:\src\wsjtx-prefix\hamlib\bin\libhamlib-4.dll"
  if (Test-Path $srcDll) { Copy-Item $srcDll $destDll }
}

# README for operators
@"
# $Product $Version (portable Windows x86_64)

WSJT-X + TX Inhibit.

## Run
1. Unzip anywhere (e.g. C:\WSJT\$Product).
2. Run bin\wsjtx.exe
3. Settings → Radio: PTT method RTS or DTR (real COMx — may be same COM as CAT).

## Local inhibit test
With wsjtx running, run bin\inhibit-test.exe and hold the grave/backtick key
(` , left of the 1 key) to hold/release. Not the Spacebar.
(Default UDP 127.0.0.1:22372.) See docs/TX_INHIBIT.md in the source repo.

https://github.com/wa1hco/wsjtx-inhibit

Not an official WSJT-X release. GPL-3.
"@ | Set-Content -Path (Join-Path $staging "README-PORTABLE.txt") -Encoding UTF8

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $staging -DestinationPath $zip -Force
$item = Get-Item $zip
Write-Host ("Created {0} ({1:N1} MB)" -f $item.FullName, ($item.Length / 1MB))
