# Install wsjtx-inhibit (testers)

**wsjtx-inhibit** is WSJT-X with a **TX Inhibit** path for multi-op / co-band
use (e.g. [WIMS](https://github.com/wa1hco/WIMS)). It is **not** an official
WSJT-X release.

**Downloads:** [GitHub Releases](https://github.com/wa1hco/wsjtx-inhibit/releases)

Pick your OS below. Prefer the **latest pre-release or release** that lists
assets for your platform.

---

## Windows

### Recommended: NSIS installer

1. Download the file ending in **`-win64.exe`** (or `*windows*installer*.exe`).
2. Run it (accept UAC if prompted).
3. Default install location is under **`C:\WSJT\`** (usually `C:\WSJT\wsjtx`).
4. Start **WSJT-X** from the Start menu or the install folder’s `bin\wsjtx.exe`.

### Portable ZIP (no installer)

1. Download the **`.zip`** for Windows.
2. Unzip anywhere (e.g. `C:\WSJT\wsjtx-inhibit`).
3. Run `bin\wsjtx.exe`.

### Quick check

- **Help → About** should show version **3.0.x** (devel/rc as tagged).
- For inhibit testing: set **PTT method = RTS or DTR** on a USB-serial port
  (see [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md)).

More detail: [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md).

---

## Linux

Three package types ship from CI. Use **one**:

| Package | Best for | Distro-agnostic? |
|---------|----------|------------------|
| **AppImage** | Any modern x86_64 or aarch64 Linux | **Yes (preferred)** |
| **`.deb`** | Debian, Ubuntu, Mint, Pop!_OS, … | No — Debian family |
| **`.rpm`** | Fedora, RHEL, Rocky, openSUSE, … | No — RPM family |

### AppImage (recommended)

1. Download `wsjtx-<version>-linux-x86_64.AppImage`
   (or `…-linux-aarch64.AppImage` on ARM).
2. Make it executable and run:

```bash
chmod +x wsjtx-*-linux-*.AppImage
./wsjtx-*-linux-*.AppImage
```

If your desktop blocks FUSE/AppImages:

```bash
./wsjtx-*-linux-*.AppImage --appimage-extract-and-run
```

No root required. Qt and most runtime libs are bundled.

### Debian / Ubuntu (`.deb`)

```bash
# example name from the release assets
sudo apt install ./wsjtx-<version>-linux-x86_64.deb
# or:
sudo dpkg -i ./wsjtx-<version>-linux-x86_64.deb
sudo apt-get install -f   # if dpkg reports missing deps
```

Then run `wsjtx` from the menu or terminal.

**Note:** The package name is still `wsjtx` (upstream CPack). It can
conflict with a distro `wsjtx` package — remove the distro package first
if `dpkg` complains.

### Fedora / RHEL / openSUSE (`.rpm`)

```bash
sudo dnf install ./wsjtx-<version>-linux-x86_64.rpm
# or older systems:
sudo rpm -Uvh ./wsjtx-<version>-linux-x86_64.rpm
```

Same conflict note as `.deb` if a distro `wsjtx` is installed.

### Quick check

```bash
wsjtx &    # or launch the AppImage
```

- **Help → About** → 3.0.x as tagged.
- Inhibit: PTT **RTS/DTR** + UDP hold on port **22372** (see
  [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md)).

More detail: [INSTALL-LINUX.md](INSTALL-LINUX.md).

---

## macOS

When a full multi-platform release is published (`build/v*` tags), `.pkg`
installers appear on the same Releases page (Apple Silicon and Intel).

---

## What this build adds (all platforms)

- Low-latency **TX Inhibit** (UDP + serial PTT path).
- Stock WSJT-X modes otherwise.

Report issues against **[wa1hco/wsjtx-inhibit](https://github.com/wa1hco/wsjtx-inhibit)**,
not the official WSJT-X project.

## Maintainers: how packages are built

| Platform | CI workflow | Artifacts |
|----------|-------------|-----------|
| Windows | `build-windows.yml` | NSIS `.exe` |
| Linux | `build-linux.yml` | AppImage, `.deb`, `.rpm` |
| Full release | tag `build/vX.Y.Z` → `release.yml` | all of the above + macOS |
| Tester-focused | tag `packages/vX.Y.Z` or **Actions → Tester packages** | Windows + Linux x86_64 |
