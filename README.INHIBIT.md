# wsjtx-inhibit — WSJT-X with low-latency TX Inhibit

**Repository:** https://github.com/wa1hco/wsjtx-inhibit  

WSJT-X mainline plus **TX Inhibit**: when a **KEY agent** **tells WSJT-X stations not to transmit** (priority radio keyed), co-band stations (app + PC + network + radio + antenna; may be remote go-boxes) will not **assert PTT** within milliseconds — **without** aborting FT8 sequencing (Halt Tx ends the QSO; TX Inhibit only filters whether to **assert PTT**).

A KEY agent senses the priority KEY and sends the UDP protocol in [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).

---

## Operators / testers — start here

| Go to | Purpose |
|-------|---------|
| **[INSTALL.md](INSTALL.md)** | **Main install guide** (all platforms) |
| **[Releases](https://github.com/wa1hco/wsjtx-inhibit/releases)** | **Download** the `.exe`, `.zip`, AppImage, `.deb`, `.rpm` (see **Assets** on a release) |
| [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md) | Windows step-by-step |
| [INSTALL-LINUX.md](INSTALL-LINUX.md) | Linux step-by-step |
| [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md) | Glossary, WSJT-X station behaviour, KEY agent, UDP protocol, smoke test |

**Important:** Install files live under each release’s **Assets** list. They are not loose files inside the source code folders. If your OS is missing from Assets, that package was not published on that release.

Do **not** use the plain file named [`INSTALL`](INSTALL) for binary install — that is upstream “build from source” text.

---

## Maintainers / developers

| Doc | Content |
|-----|---------|
| [docs/BUILDING.md](docs/BUILDING.md) | Build from source |
| [docs/SUPERBUILD.md](docs/SUPERBUILD.md) | Superbuild vs application tree |
| [scripts/windows/](scripts/windows/) | Windows stage / package helpers |
| Upstream [README.md](README.md) | Stock WSJT-X description |
| [UPSTREAM.md](UPSTREAM.md) | Baseline pin |

**Tag scheme.** The tag decides both which workflow runs and the release channel
compiled into the binary (`set_build_type()` → `-devel` / `-rcN` / GA). A tag that
matches neither pattern below runs nothing.

| Tag | Workflow | Channel | Artifacts |
|-----|----------|---------|-----------|
| `build/v3.0.2` | `release.yml` | **GA** | All platforms + source tarball + public release |
| `build/v3.0.2-rc2` | `release.yml` | **RC 2** | All platforms, prerelease |
| `packages/v3.0.2-rc2` | `tester-packages.yml` | **RC 2** | Windows + Linux x86_64, faster |
| `packages/v3.0.2-dev1` | `tester-packages.yml` | DEVEL | Windows + Linux x86_64 |

```bash
# Release candidate, all platforms (preferred for an rc)
git tag build/v3.0.2-rc2 && git push origin build/v3.0.2-rc2

# Faster tester packages (Windows + Linux x86_64 only)
git tag packages/v3.0.2-rc2 && git push origin packages/v3.0.2-rc2
# or: Actions → "Tester packages" → Run workflow
```

Product name stays `wsjtx-inhibit` across every release; only the version carries the
stage. Artifacts are named `wsjtx-inhibit-<version>-win64.exe` and install to
`C:\WSJT\wsjtx-inhibit`.

---

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
