# Build wsjtx-inhibit on Windows (MinGW + Qt5).
# Prereqs: Qt MinGW, CMake, MinGW g++, Fortran, Hamlib, FFTW, Boost, libusb on CMAKE_PREFIX_PATH.
# Example after Hamlib SDK + Qt:
#   powershell -ExecutionPolicy Bypass -File C:\src\wsjtx-inhibit\scripts\windows\Build-Inhibit.ps1
$ErrorActionPreference = 'Stop'
$Src = if ($env:WSJTX_INHIBIT_SRC) { $env:WSJTX_INHIBIT_SRC } else { 'C:\src\wsjtx-inhibit' }
$Build = if ($env:WSJTX_INHIBIT_BUILD) { $env:WSJTX_INHIBIT_BUILD } else { 'C:\build\wsjtx-inhibit' }
$Prefix = if ($env:WSJTX_INHIBIT_PREFIX) { $env:WSJTX_INHIBIT_PREFIX } else { 'C:\WSJT\wsjtx-inhibit' }
$Log = 'C:\WIMS-lab\build-inhibit.log'
New-Item -ItemType Directory -Force -Path (Split-Path $Log), $Build, $Prefix | Out-Null
function Log($m) { $l = "$(Get-Date -Format o) $m"; Add-Content $Log $l; Write-Host $l }

if (-not (Test-Path "$Src\CMakeLists.txt")) { throw "Source missing: $Src" }
if (-not (Get-Command cmake -EA SilentlyContinue)) { throw 'cmake not on PATH' }

# Prefer Qt MinGW qmake for prefix discovery
$qmake = Get-ChildItem 'C:\Qt' -Recurse -Filter qmake.exe -EA SilentlyContinue |
  Where-Object { $_.FullName -match 'mingw' } |
  Select-Object -First 1
if (-not $qmake) { throw 'qmake.exe not found under C:\Qt (install Qt 5.15 MinGW)' }
$QtPrefix = $qmake.Directory.Parent.FullName
Log "Qt: $QtPrefix"

$mingwBin = Get-ChildItem 'C:\Qt\Tools' -Directory -Filter 'mingw*' -EA SilentlyContinue |
  ForEach-Object { Join-Path $_.FullName 'bin' } |
  Where-Object { Test-Path (Join-Path $_ 'g++.exe') } |
  Select-Object -First 1
if (-not $mingwBin) { throw 'MinGW g++ not found under C:\Qt\Tools' }
$env:Path = "$mingwBin;$env:Path"
Log "MinGW: $mingwBin"

$prefixes = @($QtPrefix)
foreach ($p in @(
  'C:\hamlib-prefix',
  'C:\Hamlib',
  "$env:USERPROFILE\local\hamlib\mingw64\release",
  'C:\Tools\fftw-3.3.5-dll64',
  'C:\Boost'
)) {
  if (Test-Path $p) { $prefixes += $p }
}
$cmakePrefix = ($prefixes -join ';')
Log "CMAKE_PREFIX_PATH=$cmakePrefix"

Push-Location $Build
try {
  $args = @(
    $Src,
    '-G', 'MinGW Makefiles',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_INSTALL_PREFIX=$Prefix",
    "-DCMAKE_PREFIX_PATH=$cmakePrefix",
    '-DWSJT_SKIP_MANPAGES=ON',
    '-DWSJT_GENERATE_DOCS=OFF'
  )
  Log "cmake $($args -join ' ')"
  & cmake @args
  if ($LASTEXITCODE -ne 0) { throw "cmake failed $LASTEXITCODE" }
  $j = [Environment]::ProcessorCount
  Log "mingw32-make -j$j"
  & mingw32-make -j$j
  if ($LASTEXITCODE -ne 0) { throw "build failed $LASTEXITCODE" }
  Log 'mingw32-make install'
  & mingw32-make install
  if ($LASTEXITCODE -ne 0) { throw "install failed $LASTEXITCODE" }
  Log "SUCCESS -> $Prefix"
  # Portable zip next to install
  $zip = "$Prefix-win64-portable.zip"
  if (Test-Path $zip) { Remove-Item $zip -Force }
  if (Get-Command Compress-Archive -EA SilentlyContinue) {
    Compress-Archive -Path (Join-Path $Prefix '*') -DestinationPath $zip -Force
    Log "portable zip: $zip"
  }
  'build-ok' | Set-Content 'C:\WIMS-lab\INHIBIT_BUILD_OK.txt'
} finally {
  Pop-Location
}
