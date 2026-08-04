# Windows install system — wsjtx-mainline-wims

This repository is the **source of truth** for the Windows build of
**WSJT-X mainline + WIMS low-latency TX Inhibit** (product name:
`wsjtx-mainline-wims`).

## What operators install

| Artifact | How you get it | Notes |
|----------|----------------|-------|
| **NSIS installer** `wsjtx-<ver>-win64.exe` | [GitHub Releases](https://github.com/wa1hco/wsjtx-mainline-wims/releases) | Preferred for seats |
| **Portable stage ZIP** | Same Releases, or local `scripts/windows` | Unzip and run `bin\wsjtx.exe` |
| **Build from source** | This repo + MSYS2 | See [docs/BUILDING.md](docs/BUILDING.md) |

Releases are produced by [`.github/workflows/release.yml`](.github/workflows/release.yml)
when a tag matching `build/v*` is pushed.

## Low-latency TX Inhibit (included)

Every Windows build from `main` includes the in-process **TxInhibit gate**:

- UDP hold/keepalive on port **22372** (or ephemeral if busy)
- Physical PTT = RTS/DTR intent ∧ ¬inhibit (milliseconds, not Halt Tx)
- See [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md)

## Publish a Windows (and multi-platform) release

```bash
# VERSION must match CMakeLists.txt project VERSION first three components
# Example CMake: VERSION 3.0.2.0  →  tags build/v3.0.2 or build/v3.0.2-rc1
git tag build/v3.0.2-rc1
git push origin build/v3.0.2-rc1
```

CI builds Hamlib, WSJT-X, runs tests, packages **NSIS** for Windows, and
attaches installers to the GitHub Release. That Release page is the install
system front door for WIMS seats.

## Local MSYS2 stage (developers / this VM)

Prerequisites: MSYS2 MINGW64 toolchain, Qt5, FFTW, Boost, OmniRig, Hamlib
prefix with **both** `libhamlib-4.dll` **and** `rigctl.exe` installed
(`make install-strip` must install `tests/` tools).

```bash
# From MINGW64 shell, after Hamlib is installed into e.g. /c/src/wsjtx-prefix/hamlib
export PATH="/c/src/wsjtx-prefix/hamlib/bin:/mingw64/bin:/usr/bin:$PATH"
cmake -G "MSYS Makefiles" -S /c/src/wsjtx-wims -B /c/src/wsjtx-prefix/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/c/src/wsjtx-prefix/hamlib" \
  -DOMNIRIG_TYPE_LIB="/c/Program Files (x86)/Afreet/OmniRig/OmniRig.exe" \
  -DWSJT_SKIP_MANPAGES=ON -DWSJT_GENERATE_DOCS=OFF -DWSJT_BUILD_TESTS=OFF \
  -DWSJT_RELEASE_CHANNEL=DEVEL -Wno-dev
cmake --build /c/src/wsjtx-prefix/build -j$(nproc)
cmake --install /c/src/wsjtx-prefix/build --prefix /c/src/wsjtx-prefix/stage
```

Or use the helper scripts:

| Script | Purpose |
|--------|---------|
| [`scripts/windows/Build-Stage.ps1`](scripts/windows/Build-Stage.ps1) | Drive MSYS2 full stage build from PowerShell |
| [`scripts/windows/Install-FromStage.ps1`](scripts/windows/Install-FromStage.ps1) | Copy stage → install dir + desktop shortcut |
| [`scripts/windows/Package-PortableZip.ps1`](scripts/windows/Package-PortableZip.ps1) | Zip stage for distribution |

## Seat layout (WIMS)

Typical multi-seat Windows install:

```
C:\WSJT\wsjtx-mainline-wims\   ← this product (not stock WSJT-X)
  bin\wsjtx.exe
  bin\jt9.exe
  bin\rigctl-wsjtx.exe
  ...
```

WIMS launcher scripts should point at **this** `wsjtx.exe`, not the
stock SourceForge build, so the low-latency inhibit thread is present.

## Versioning

| Piece | Rule |
|-------|------|
| `CMakeLists.txt` `VERSION` | Numeric only (e.g. `3.0.2.0`) |
| Git tag | `build/v3.0.2` or `build/v3.0.2-rcN` |
| Artifact names | `wsjtx-<tag-version>-win64.exe` |

## Not official WSJT-X

GPL-3 same as upstream. Not an ARRL / WSJT Development Group release.
For multi-instance contest use with [WIMS](https://github.com/wa1hco/WIMS).
