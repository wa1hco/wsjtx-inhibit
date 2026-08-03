# wsjtx-wims — WSJT-X with WIMS TX Inhibit

**Repository:** https://github.com/wa1hco/wsjtx-wims  

Fork of official [WSJT-X](https://github.com/WSJTX/wsjtx) for the
[WIMS](https://github.com/wa1hco/WIMS) multi-instance VHF contest console.

Adds a **low-latency TX Inhibit gate**: when an SSB/CW operator keys, co-band
digital radios stop **radiating** within milliseconds **without** aborting FT8
sequencing (Halt Tx is the wrong instrument).

| Doc | Content |
|-----|---------|
| [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md) | Patch behaviour & setup |
| [docs/SUPERBUILD.md](docs/SUPERBUILD.md) | Superbuild vs application tree |
| [docs/BUILDING.md](docs/BUILDING.md) | Linux / Windows build |
| Upstream [README.md](README.md) | Stock WSJT-X description |
| [UPSTREAM.md](UPSTREAM.md) | Baseline pin |

## Test binaries

GitHub **Releases** carry pre-built executables for testing (from CI).

- Trigger a release by pushing a tag: `build/vX.Y.Z` or `build/vX.Y.Z-rcN`
  (must match `CMakeLists.txt` VERSION).
- Workflow: [`.github/workflows/release.yml`](.github/workflows/release.yml)
  builds Linux, Windows, and macOS and attaches installers/archives.

For ad-hoc CI artifacts without a formal release, use the `ci.yml` workflow
run downloads.

## Branches / tags

| Ref | Meaning |
|-----|---------|
| `baseline-v3.0.2` | Clean mainline import, no WIMS code |
| `main` | Baseline + TX Inhibit |

## License

GNU GPL v3 — same as WSJT-X (`COPYING`). WIMS patches are GPL-3.0-or-later.
Not an official WSJT-X release.
