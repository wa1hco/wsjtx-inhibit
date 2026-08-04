# wsjtx-inhibit — WSJT-X with low-latency TX Inhibit

**Repository:** https://github.com/wa1hco/wsjtx-inhibit  

WSJT-X mainline plus an in-process **TX Inhibit** path: co-band digital
stations stop **radiating** within milliseconds when another operator keys,
**without** aborting FT8 sequencing (Halt Tx is the wrong instrument).

[WIMS](https://github.com/wa1hco/WIMS) is one multi-seat consumer of this
gate. The feature is useful anywhere multi-op / co-band PTT interlock is needed.

| Doc | Content |
|-----|---------|
| **[INSTALL-WINDOWS.md](INSTALL-WINDOWS.md)** | Windows install system (Releases, NSIS, portable ZIP) |
| [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md) | Gate behaviour & operator setup |
| [docs/SUPERBUILD.md](docs/SUPERBUILD.md) | Superbuild vs application tree |
| [docs/BUILDING.md](docs/BUILDING.md) | Linux / Windows build from source |
| [scripts/windows/](scripts/windows/) | Stage build, install, portable package helpers |
| Upstream [README.md](README.md) | Stock WSJT-X description |
| [UPSTREAM.md](UPSTREAM.md) | Baseline pin |

## Install (Windows)

This repo is the **source of truth** for Windows builds and installers:

1. **Operators** download the NSIS installer or portable ZIP from
   [GitHub Releases](https://github.com/wa1hco/wsjtx-inhibit/releases).
2. **CI** builds installers on tag `build/vX.Y.Z` / `build/vX.Y.Z-rcN`
   via [`.github/workflows/release.yml`](.github/workflows/release.yml).
3. **Developers** use MSYS2 stage builds and
   [`scripts/windows`](scripts/windows) under `C:\WSJT\wsjtx-inhibit\`.

Details: **[INSTALL-WINDOWS.md](INSTALL-WINDOWS.md)**.

```bash
# Publish installers (VERSION must match CMakeLists.txt first three components)
git tag build/v3.0.2-rc1
git push origin build/v3.0.2-rc1
```

## Former names

| Old slug | Status |
|----------|--------|
| `wa1hco/wsjtx-mainline-wims` | Renamed → **`wsjtx-inhibit`** (GitHub redirects) |
| `wa1hco/wsjtx-wims` | Legacy; archived pointer to this repo |

## Branches / tags

| Ref | Meaning |
|-----|---------|
| `baseline-v3.0.2` | Clean mainline import, no inhibit code |
| `main` | Mainline + TX Inhibit |

## License

GNU GPL v3 — same as WSJT-X (`COPYING`). Inhibit patches are GPL-3.0-or-later.
Not an official WSJT-X release.
