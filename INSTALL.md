# How to install and try wsjtx-inhibit

**You are an operator / tester.** You do not need to build anything from source.

**wsjtx-inhibit** is a modified **WSJT-X** with a **TX Inhibit** feature for multi-op and same-band stations (for example with [WIMS](https://github.com/wa1hco/WIMS)). It is **not** an official WSJT-X / ARRL release.

---

## 1. Where the install files are

All ready-to-run packages are on the **GitHub Releases** page — not in this source tree, and not on SourceForge.

### Open this page

**[https://github.com/wa1hco/wsjtx-inhibit/releases](https://github.com/wa1hco/wsjtx-inhibit/releases)**

1. Click the **latest** release (or the pre-release your tester email named).
2. Scroll to the **Assets** section at the bottom of that release.
3. Download the file for your computer (see tables below).

If **Assets** is empty or has no file for your OS, that package is not published yet — see [What is available today](#what-is-available-today).

---

## What is available today

Check **Assets** on the release you opened. Currently typical:

| Your computer | What you should see under Assets | Status |
|---------------|----------------------------------|--------|
| **Windows 64-bit** | `…-win64.exe` and/or `…-windows-x86_64.zip` | **Published** on recent test releases |
| **Linux** (AppImage / `.deb` / `.rpm`) | Names ending in `.AppImage`, `.deb`, or `.rpm` | Only if that release lists them — **not every release has Linux yet** |
| **macOS** | `.pkg` files | Only on full multi-platform releases |

**If Linux or macOS files are missing from Assets**, there is nothing to download for that OS on that release. Ask the maintainer for a Linux/macOS build, or wait for a release that lists those files.

You never need to compile code or install a tool called “NSIS.” The Windows `.exe` **is** the installer program (double-click it). “NSIS” is only the software *developers* use to *create* that `.exe`.

---

## 2. Windows (operators)

**Detailed steps:** [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md)

### Pick one file from Assets

| File name looks like | What to do |
|----------------------|------------|
| **`something-win64.exe`** | **Recommended.** Double-click to install (like other Windows programs). |
| **`something-windows-x86_64.zip`** | Unzip to a folder, then run `bin\wsjtx.exe` (no installer). |

Filenames may still contain `mainline-wims` from an older project name. That is still **wsjtx-inhibit**.

### Short install (`.exe`)

1. Close any running WSJT-X.
2. Download the **`-win64.exe`** from **Assets**.
3. Run it. If Windows SmartScreen appears: **More info** → **Run anyway** (only if you trust this GitHub project).
4. Finish the installer (often under `C:\WSJT\…`).
5. Start from the Start menu, or open the install folder and run `bin\wsjtx.exe`.

### After install — use it like WSJT-X

Set callsign, radio, audio as usual under **File → Settings**.

**For TX Inhibit testing**, set **PTT method** to **RTS** or **DTR** on a USB serial PTT port (not CAT-PTT only). Details: [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md).

Confirm: **Help → About** shows version **3.0.x** from this project.

---

## 3. Linux (operators)

**Detailed steps:** [INSTALL-LINUX.md](INSTALL-LINUX.md)

### First: confirm Linux packages exist

On the [Releases](https://github.com/wa1hco/wsjtx-inhibit/releases) page, open a release and look under **Assets** for one of:

| File ends with | Use if you… |
|----------------|-------------|
| **`.AppImage`** | Want the simplest option on most 64-bit Linux PCs (**preferred**) |
| **`.deb`** | Use Debian, Ubuntu, Linux Mint, Pop!_OS, etc. |
| **`.rpm`** | Use Fedora, Rocky, RHEL, openSUSE, etc. |

**If none of those appear under Assets**, this release has no Linux build. Do not look for them in the git repo folders.

### AppImage (when the file is in Assets)

```bash
chmod +x wsjtx-*-linux-*.AppImage
./wsjtx-*-linux-*.AppImage
```

If the desktop blocks it:

```bash
./wsjtx-*-linux-*.AppImage --appimage-extract-and-run
```

### Debian / Ubuntu (`.deb` in Assets)

```bash
sudo apt install ./wsjtx-*-linux-*.deb
# if needed:
sudo apt-get install -f
```

Then start `wsjtx` from the menu or terminal.  
If you already have distro package `wsjtx`, remove it first if the install conflicts.

### Fedora / RHEL / openSUSE (`.rpm` in Assets)

```bash
sudo dnf install ./wsjtx-*-linux-*.rpm
# or: sudo rpm -Uvh ./wsjtx-*-linux-*.rpm
```

---

## 4. macOS (operators)

When a release’s **Assets** list includes `.pkg` files, download the one for your Mac (Apple Silicon or Intel), open it, and follow the installer.  
If there is no `.pkg` in Assets, macOS is not available on that release.

---

## 5. What TX Inhibit is (one paragraph)

WSJT-X sequencing and audio stay the same. When another operator or interlock software says “hold,” this build can **stop the radio from keying** on the **RTS/DTR PTT line** within milliseconds. That is **not** the same as **Halt Tx** (which aborts the QSO sequence).

---

## 6. Problems and feedback

- Installer / SmartScreen / antivirus issues  
- Radio and PTT method (RTS/DTR port?)  
- Whether FT8 works like normal WSJT-X  
- Whether inhibit holds and releases correctly  

Report here: [https://github.com/wa1hco/wsjtx-inhibit/issues](https://github.com/wa1hco/wsjtx-inhibit/issues)  
Do **not** report this fork to the official WSJT-X project as a stock bug.

---

## For maintainers only (building packages)

Operators can ignore this section.

| How packages get onto the Releases page | Artifacts |
|-----------------------------------------|-----------|
| Tag `build/v…` or CI release workflow | Windows installer + ZIP; Linux AppImage/deb/rpm when that job runs; macOS when configured |
| Tag `packages/v…` or **Actions → Tester packages** | Often Windows + Linux x86_64 only |

Maintainer detail: [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md) (bottom), [INSTALL-LINUX.md](INSTALL-LINUX.md) (bottom), [docs/BUILDING.md](docs/BUILDING.md).
