#Requires -Version 5.1
<#
.SYNOPSIS
  Install a built stage tree as the seat-local wsjtx-mainline-wims product.
#>
param(
  [string]$StageDir = "C:\src\wsjtx-prefix\stage",
  [string]$InstallDir = "C:\WSJT\wsjtx-mainline-wims",
  [switch]$DesktopShortcut,
  [switch]$Force
)

$ErrorActionPreference = "Stop"
$wsjtx = Join-Path $StageDir "bin\wsjtx.exe"
if (-not (Test-Path $wsjtx)) { throw "Missing $wsjtx - build stage first." }

if ((Test-Path $InstallDir) -and -not $Force) {
  throw "Install dir exists: $InstallDir (pass -Force to overwrite)"
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Write-Host "Copying stage -> $InstallDir"
robocopy $StageDir $InstallDir /MIR /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
# robocopy codes 0-7 are success
if ($LASTEXITCODE -ge 8) { throw "robocopy failed: $LASTEXITCODE" }

# Ensure Hamlib DLL next to binaries if present in stage or sibling hamlib prefix
$hamDll = Join-Path $StageDir "bin\libhamlib-4.dll"
if (-not (Test-Path $hamDll)) {
  $alt = "C:\src\wsjtx-prefix\hamlib\bin\libhamlib-4.dll"
  if (Test-Path $alt) {
    Copy-Item $alt (Join-Path $InstallDir "bin\libhamlib-4.dll") -Force
  }
}

$installed = Join-Path $InstallDir "bin\wsjtx.exe"
if (-not (Test-Path $installed)) { throw "Install incomplete: $installed missing" }

if ($DesktopShortcut) {
  $desk = [Environment]::GetFolderPath("Desktop")
  $lnkPath = Join-Path $desk "WSJT-X Mainline WIMS.lnk"
  $w = New-Object -ComObject WScript.Shell
  $s = $w.CreateShortcut($lnkPath)
  $s.TargetPath = $installed
  $s.WorkingDirectory = (Join-Path $InstallDir "bin")
  $s.Description = "WSJT-X mainline + WIMS low-latency TX Inhibit"
  $s.Save()
  Write-Host "Desktop shortcut: $lnkPath"
}

Write-Host "Installed: $installed"
Write-Host "Done."
