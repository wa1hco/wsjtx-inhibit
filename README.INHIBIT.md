# wsjtx-inhibit — WSJT-X with low-latency TX Inhibit

**Repository:** https://github.com/wa1hco/wsjtx-inhibit  
**This branch:** official [WSJT-X v3.3.0-beta4](https://github.com/WSJTX/wsjtx/releases/tag/v3.3.0-beta4) plus the sole-writer TX Inhibit path.

WSJT-X v3.3.0-beta4 already implements type 17 / type 18 TX Inhibit. This branch adds the low-latency pin path: the UDP dispatch thread is the only Unix writer of RTS/DTR, and type 17 carries `t_rx_ns` / `t_pin_ns`. A **KEY agent** tells co-band stations to inhibit transmit without aborting the FT8 sequence (Halt Tx ends the QSO; TX Inhibit only holds the PTT pin).

Arming is upstream’s rule, not a separate checkbox: PTT method RTS or DTR, and Accept UDP requests on. Point the KEY agent at the Heartbeat source `host:port` and that message’s Id (`WSJT-X` for the default instance). `inhibit-test` and `tools/send_inhibit_hold.py` learn both from the UDP server stream. `inhibit-agent` takes `--addr host:port` and `--id`.

A **KEY agent** (the role) senses the priority KEY and sends the UDP protocol in [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md). This tree ships **`inhibit-agent`** (operator supplies serial port and dest). The WIMS tree ships **`wims-key-agent`** (destinations from WIMS discovery).

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
| `build/v3.0.2-rc5` | `release.yml` | **RC 5** | All platforms, prerelease |
| `packages/v3.0.2-rc5` | `tester-packages.yml` | **RC 5** | Windows + Linux x86_64, faster |
| `packages/v3.0.2-dev1` | `tester-packages.yml` | DEVEL | Windows + Linux x86_64 |

```bash
# Release candidate, all platforms (preferred for an rc)
git tag build/v3.0.2-rc5 && git push origin build/v3.0.2-rc5

# Faster tester packages (Windows + Linux x86_64 only)
git tag packages/v3.0.2-rc5 && git push origin packages/v3.0.2-rc5
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
| `wa1hco/wsjtx-improved-wims` | Obsolete; superseded by **`wsjtx-improved-inhibit`** |

## Sibling trees

| Tree | Baseline | Purpose |
|------|----------|---------|
| **`wsjtx-inhibit`** (this repo) | Official mainline v3.0.2 | Reference implementation; source of the upstream PR |
| `wsjtx-improved-inhibit` | WSJT-X Improved 3.1.0 AL_PLUS_260522 | Same TX Inhibit feature carried on the Improved lineage |

## Branches / tags

| Ref | Meaning |
|-----|---------|
| `26e8bf5` (root commit) | Clean mainline v3.0.2 import, no inhibit code |
| `main` | Mainline + TX Inhibit |
| `tx-inhibit-upstream` | Rebased series for the upstream PR |

## License

GNU GPL v3 — same as WSJT-X (`COPYING`). Inhibit patches are GPL-3.0-or-later.  
Not an official WSJT-X release.
