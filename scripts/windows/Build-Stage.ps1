#Requires -Version 5.1
<#
.SYNOPSIS
  Build wsjtx-mainline-wims stage install under MSYS2 MINGW64.

.DESCRIPTION
  Expects Hamlib already installed to -HamlibPrefix (with rigctl.exe in bin).
  Produces a full cmake --install tree under -StageDir.
#>
param(
  [string]$SourceDir = "C:\src\wsjtx-wims",
  [string]$PrefixDir = "C:\src\wsjtx-prefix",
  [string]$HamlibPrefix = "C:\src\wsjtx-prefix\hamlib",
  [string]$BuildDir = "C:\src\wsjtx-prefix\build",
  [string]$StageDir = "C:\src\wsjtx-prefix\stage",
  [string]$OmniRig = "C:\Program Files (x86)\Afreet\OmniRig\OmniRig.exe",
  [string]$MsysBash = "C:\msys64\usr\bin\bash.exe",
  [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $MsysBash)) { throw "MSYS2 bash not found: $MsysBash" }
if (-not (Test-Path $SourceDir)) { throw "Source not found: $SourceDir" }
if (-not (Test-Path (Join-Path $HamlibPrefix "bin\rigctl.exe"))) {
  throw "Hamlib rigctl.exe missing under $HamlibPrefix\bin — run make install-strip in Hamlib (tests tools)."
}
if (-not (Test-Path $OmniRig)) { throw "OmniRig.exe not found: $OmniRig" }
if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }

$srcUnix = ($SourceDir -replace '\\', '/') -replace '^([A-Za-z]):', { "/$($args[0].Groups[1].Value.ToLower())" }
# cygpath-style via bash is more reliable
$script = @"
set -euo pipefail
export MSYSTEM=MINGW64
export PATH="`$(cygpath '$HamlibPrefix')/bin:/mingw64/bin:/usr/bin:`${PATH:-}"
SRC="`$(cygpath '$SourceDir')"
HAMLIB="`$(cygpath '$HamlibPrefix')"
BUILD="`$(cygpath '$BuildDir')"
STAGE="`$(cygpath '$StageDir')"
OMNI="`$(cygpath '$OmniRig')"
JOBS=$Jobs
if [ -x /mingw64/bin/dumpcpp-qt5.exe ] && [ ! -e /mingw64/bin/dumpcpp.exe ]; then
  ln -sf /mingw64/bin/dumpcpp-qt5.exe /mingw64/bin/dumpcpp.exe
fi
which rigctl
rigctl -V || true
cmake -G "MSYS Makefiles" -S "`$SRC" -B "`$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="`$HAMLIB" \
  -DOMNIRIG_TYPE_LIB="`$OMNI" \
  -DCMAKE_Fortran_FLAGS="-fallow-argument-mismatch -std=legacy" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_BUILD_TESTS=OFF \
  -DWSJT_RELEASE_CHANNEL=DEVEL \
  -Wno-dev
grep -E '^RIGCTL' "`$BUILD/CMakeCache.txt" || true
if grep -q 'RIGCTL_EXE:FILEPATH=RIGCTL_EXE-NOTFOUND' "`$BUILD/CMakeCache.txt"; then
  echo "ERROR: RIGCTL not found"; exit 1
fi
cmake --build "`$BUILD" -j"`$JOBS"
rm -rf "`$STAGE"
cmake --install "`$BUILD" --prefix "`$STAGE"
test -f "`$STAGE/bin/wsjtx.exe"
test -f "`$STAGE/bin/rigctl-wsjtx.exe" || test -f "`$STAGE/bin/rigctl.exe"
ls -la "`$STAGE/bin" | head -40
echo "STAGE OK: `$STAGE"
"@

$tmp = Join-Path $PrefixDir "Build-Stage-run.sh"
[System.IO.File]::WriteAllText($tmp, ($script -replace "`r`n", "`n"))
& $MsysBash -lc "export MSYSTEM=MINGW64 CHERE_INVOKING=1; export PATH=/mingw64/bin:/usr/bin:`$PATH; bash '$(($tmp -replace '\\','/') -replace '^([A-Za-z]):',{"/$($args[0].Groups[1].Value.ToLower())"})'"
if ($LASTEXITCODE -ne 0) { throw "Build-Stage failed: exit $LASTEXITCODE" }
Write-Host "Stage ready: $StageDir"
