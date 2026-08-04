# Linux packages — wsjtx-inhibit

CI ([`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml))
builds three installers per architecture (**x86_64** and **aarch64** on full
releases):

1. **AppImage** — portable, distro-agnostic (preferred for most testers)
2. **`.deb`** — CPack DEB (Debian/Ubuntu family)
3. **`.rpm`** — CPack RPM (Fedora/RHEL/SUSE family)

Filenames look like:

```text
wsjtx-3.0.2-linux-x86_64.AppImage
wsjtx-3.0.2-linux-x86_64.deb
wsjtx-3.0.2-linux-x86_64.rpm
```

(Version string matches the release tag / CI `version` input.)

## Which file should I give testers?

| Audience | Send |
|----------|------|
| Mixed Linux users / unknown distro | **AppImage** |
| “We’re all on Ubuntu 22.04/24.04” | **`.deb`** |
| “We’re all on Fedora / Rocky” | **`.rpm`** |

Point them at **[INSTALL.md](INSTALL.md)** for copy-paste install steps.

## AppImage notes

- Built with **linuxdeploy** + Qt5 plugin; ships Qt and common deps.
- Smoke-tested in CI for core binaries and Qt audio plugins.
- Needs a 64-bit glibc-based system (typical for AppImages from Ubuntu 24.04 runners).
- Very old distros (e.g. CentOS 7) may need the `.rpm` built on a matching
  host, or build from source ([docs/BUILDING.md](docs/BUILDING.md)).

## .deb / .rpm notes

- Produced by **CPack** after `cmake --build` (same tree as the AppImage).
- Package **name** remains upstream `wsjtx` so desktop files and paths match
  stock WSJT-X (`/usr/bin/wsjtx`). This **can conflict** with distro packages
  named `wsjtx` — uninstall the distro package first if needed.
- Dependencies are declared for common Qt5 / FFTW / Boost / portaudio stacks;
  if install fails, install distro “WSJT-X build deps” or use the AppImage.

### Local packaging (after a normal Linux build)

```bash
# from the cmake build directory
cpack -G DEB -D "CPACK_DEBIAN_FILE_NAME=wsjtx-${VERSION}-linux-$(uname -m).deb"
cpack -G RPM  -D "CPACK_PACKAGE_FILE_NAME=wsjtx-${VERSION}-linux-$(uname -m)"
```

Or use the helper:

```bash
scripts/linux/package-cpack.sh /path/to/build-dir 3.0.2
```

### AppImage locally

Requires `linuxdeploy` (see CI step **Package AppImage** in
`build-linux.yml`). Prefer CI artifacts unless you are debugging packaging.

## TX Inhibit on Linux

Same protocol as Windows: UDP JSON on port **22372**, PTT via RTS/DTR.
See [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md).
