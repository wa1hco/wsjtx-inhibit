# wsjtx-inhibit — WSJT-X with low-latency TX Inhibit

**Repository:** https://github.com/wa1hco/wsjtx-inhibit  

WSJT-X mainline plus an in-process **TX Inhibit** path: co-band digital
stations stop **radiating** within milliseconds when another operator keys,
**without** aborting FT8 sequencing (Halt Tx is the wrong instrument).

[WIMS](https://github.com/wa1hco/WIMS) is one multi-seat consumer of this
gate. The feature is useful anywhere multi-op / co-band PTT interlock is needed.

| Doc | Content |
|-----|---------|
| **[INSTALL.md](INSTALL.md)** | **Testers: Windows + Linux install (send this link)** |
| [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md) | Windows install system detail |
| [INSTALL-LINUX.md](INSTALL-LINUX.md) | AppImage / `.deb` / `.rpm` detail |
| [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md) | Gate behaviour & operator setup |
| [docs/SUPERBUILD.md](docs/SUPERBUILD.md) | Superbuild vs application tree |
| [docs/BUILDING.md](docs/BUILDING.md) | Linux / Windows build from source |
| [scripts/windows/](scripts/windows/) | Stage build, install, portable package helpers |
| Upstream [README.md](README.md) | Stock WSJT-X description |
| [UPSTREAM.md](UPSTREAM.md) | Baseline pin |

## Install (testers)

**Send testers this page:** **[INSTALL.md](INSTALL.md)**  
**Downloads:** [Releases](https://github.com/wa1hco/wsjtx-inhibit/releases)

| OS | Packages |
|----|----------|
| Windows | NSIS `.exe`, portable ZIP |
| Linux | **AppImage** (distro-agnostic), **`.deb`**, **`.rpm`** |
| macOS | `.pkg` on full `build/v*` releases |

```bash
# Full multi-platform release (incl. macOS)
git tag build/v3.0.2-rc1 && git push origin build/v3.0.2-rc1

# Faster tester packages (Windows + Linux x86_64 only)
git tag packages/v3.0.2-dev1 && git push origin packages/v3.0.2-dev1
# or: Actions → "Tester packages" → Run workflow
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
