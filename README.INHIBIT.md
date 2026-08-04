# wsjtx-inhibit — WSJT-X with low-latency TX Inhibit

**Repository:** https://github.com/wa1hco/wsjtx-inhibit  

WSJT-X mainline plus **TX Inhibit**: co-band stations stop **radiating** within milliseconds when a **KEY agent** reports that a priority radio is keyed, **without** aborting FT8 sequencing (Halt Tx ends the QSO sequence; inhibit only holds PTT).

A KEY agent is any program that senses the priority KEY line and sends the UDP hold protocol documented in [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).

---

## Operators / testers — start here

| Go to | Purpose |
|-------|---------|
| **[INSTALL.md](INSTALL.md)** | **Main install guide** (all platforms) |
| **[Releases](https://github.com/wa1hco/wsjtx-inhibit/releases)** | **Download** the `.exe`, `.zip`, AppImage, `.deb`, `.rpm` (see **Assets** on a release) |
| [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md) | Windows step-by-step |
| [INSTALL-LINUX.md](INSTALL-LINUX.md) | Linux step-by-step |
| [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md) | Gate behaviour, KEY agent design, UDP protocol, smoke test |

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

```bash
# Full multi-platform release (when CI is configured for it)
git tag build/v3.0.2-rc1 && git push origin build/v3.0.2-rc1

# Faster tester packages (often Windows + Linux x86_64)
git tag packages/v3.0.2-dev1 && git push origin packages/v3.0.2-dev1
# or: Actions → "Tester packages" → Run workflow
```

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
