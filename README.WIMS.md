# wsjtx-mainline-wims — WSJT-X mainline + WIMS low-latency TX Inhibit

**Install / release source:** https://github.com/wa1hco/wsjtx-mainline-wims  
**Legacy clone URL** (same project history): https://github.com/wa1hco/wsjtx-wims

Product name for Windows seats: **`wsjtx-mainline-wims`**.

Fork of official [WSJT-X](https://github.com/WSJTX/wsjtx) for the
[WIMS](https://github.com/wa1hco/WIMS) multi-instance VHF contest console.

Adds a **low-latency TX Inhibit gate** (dedicated thread): when an SSB/CW
operator keys, co-band digital radios stop **radiating** within milliseconds
**without** aborting FT8 sequencing (Halt Tx is the wrong instrument).

| Doc | Content |
|-----|---------|
| **[INSTALL-WINDOWS.md](INSTALL-WINDOWS.md)** | **Windows install system** (Releases, NSIS, portable ZIP, seats) |
| [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md) | Gate behaviour & operator setup |
| [docs/SUPERBUILD.md](docs/SUPERBUILD.md) | Superbuild vs application tree |
| [docs/BUILDING.md](docs/BUILDING.md) | Linux / Windows build from source |
| [scripts/windows/](scripts/windows/) | Stage build, install, portable package helpers |
| Upstream [README.md](README.md) | Stock WSJT-X description |
| [UPSTREAM.md](UPSTREAM.md) | Baseline pin |

## Install system (Windows)

This repo **is** the source of the Windows install pipeline:

1. **Operators** download the NSIS installer or portable ZIP from
   [GitHub Releases](https://github.com/wa1hco/wsjtx-mainline-wims/releases).
2. **CI** builds installers on tag `build/vX.Y.Z` / `build/vX.Y.Z-rcN`
   via [`.github/workflows/release.yml`](.github/workflows/release.yml)
   (Windows NSIS + Linux AppImage + macOS pkg).
3. **Developers** use MSYS2 stage builds and
   [`scripts/windows`](scripts/windows) to install under
   `C:\WSJT\wsjtx-mainline-wims\`.

Details: **[INSTALL-WINDOWS.md](INSTALL-WINDOWS.md)**.

```bash
# Publish installers (VERSION must match CMakeLists.txt first three components)
git tag build/v3.0.2-rc1
git push origin build/v3.0.2-rc1
```

## Branches / tags

| Ref | Meaning |
|-----|---------|
| `baseline-v3.0.2` | Clean mainline import, no WIMS code |
| `main` | Mainline + low-latency TX Inhibit thread |

## License

GNU GPL v3 — same as WSJT-X (`COPYING`). WIMS patches are GPL-3.0-or-later.
Not an official WSJT-X release.
