# How to install and try wsjtx-inhibit

**You are an operator / tester.** You do not need to build anything from source.

**wsjtx-inhibit** is a modified **WSJT-X** with **TX Inhibit** for multi-op and same-band stations. A **WSJT-X station** is one digi position (this app, PC, radio, antenna). A **KEY agent** (or the Spacebar test helper) **tells WSJT-X stations not to transmit** when a priority radio is keyed. It is **not** an official WSJT-X / ARRL release.

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
| **`wsjtx-inhibit-…-win64.exe`** (or any `…-win64.exe`) | **Recommended.** Double-click to install (like other Windows programs). |
| **`wsjtx-inhibit-…-windows-x86_64.zip`** (or any Windows `…zip`) | Unzip to a folder, then run `bin\wsjtx.exe` (no installer). |

Example release: **[wsjtx-inhibit-rc1](https://github.com/wa1hco/wsjtx-inhibit/releases/tag/wsjtx-inhibit-rc1)** (`wsjtx-inhibit-rc1-win64.exe` / `…-windows-x86_64.zip`).

### Short install (`.exe`)

1. Close any running WSJT-X.
2. Download the **`-win64.exe`** from **Assets**.
3. Run it. If Windows SmartScreen appears: **More info** → **Run anyway** (only if you trust this GitHub project).
4. Finish the installer (often under `C:\WSJT\…`).
5. Start from the Start menu, or open the install folder and run `bin\wsjtx.exe`.

### After install — use it like WSJT-X

Set callsign, radio, audio as usual under **File → Settings**.

**For TX Inhibit testing** (required checklist):

1. **PTT method** = **RTS** or **DTR** (not **CAT** method, not **VOX** alone).
2. **Enable TX Inhibit** = checked (Settings → Radio; default is **off**).
3. **PTT port** = a real serial device (`COMx` on Windows, `/dev/ttyUSBx` on Linux) — **not** the special list value **CAT**.  
   That COM may be the **same** as the CAT port (shared USB CAT + RTS/DTR — valid and common) or a separate PTT adapter.
4. Wire RTS or DTR to the radio’s PTT/SEND (or use the radio’s USB SEND / PC KEYING map). Turn **radio VOX off** for tests so only the key line can key the rig.
5. When the KEY agent has said **not to transmit**, the **status bar** shows a red **TX INHIBITED** badge. Software sequencing/audio may continue; this WSJT-X station does not **assert PTT**.

**Shared radio USB (CAT + RTS/DTR on one COM):** Valid on Icom / Elecraft / Yaesu and similar. Use **Handshake = None**, map the line to SEND/PTT (not flow control), and let only one app drive the modem lines. Brand menus and pitfalls:  
[docs/TX_INHIBIT.md — Shared USB CAT + RTS/DTR](docs/TX_INHIBIT.md#shared-usb-cat--rtsdtr-what-operators-actually-do).

Design (TX Inhibit + KEY agent): [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).  
Windows/Linux step-by-step: [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md), [INSTALL-LINUX.md](INSTALL-LINUX.md).

**Confirm you launched this build:** start the copy you just installed. **Help → About** shows **wsjtx-inhibit** and base version **3.0.x** (mainline + TX Inhibit). The main window title also starts with **wsjtx-inhibit**.

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

WSJT-X sequencing and audio stay the same. When a KEY agent (or the Spacebar helper) **tells the WSJT-X station not to transmit**, this build does not **assert PTT** (RTS/DTR) within milliseconds. That is **not** **Halt Tx** (which aborts the QSO sequence). Those requests arrive as short UDP messages (default port **22372**); they expire unless refreshed. Local CTS KEY sensing is **not** used (see [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md) §5).

---

## 6. Test TX Inhibit with the KEY helper (`inhibit-spacebar`)

You can exercise TX Inhibit **without** a full multi-op system by simulating a priority **KEY** line. Use the **grave/backtick** key (**`**, left of **1** on US keyboards)—**not Space**—so normal typing does not false-trigger holds.

### What you need

| Item | Notes |
|------|--------|
| **wsjtx-inhibit** running | **PTT = RTS/DTR**, **Enable TX Inhibit** on (see checklist above) |
| **KEY helper** | **`bin/inhibit-spacebar`** next to `wsjtx`, or Python `tools/send_inhibit_hold.py` |

### `inhibit-spacebar` (recommended — same folder as the app)

Installed with the package (portable ZIP / installer stage):

```text
…\bin\wsjtx.exe
…\bin\inhibit-spacebar.exe      ← Windows
…/bin/wsjtx
…/bin/inhibit-spacebar          ← Linux
```

1. Start **wsjtx-inhibit** with **PTT = RTS or DTR**, **Enable TX Inhibit** checked, real serial port.
2. Run **`bin\inhibit-spacebar.exe`** (or `./bin/inhibit-spacebar`) from that install.
3. Hold the **grave** key (`` ` ``, left of 1) → **assert KEY**. Red **TX INHIBITED**. **Not Space.**
4. Release grave → hang then **release hold**; badge clears.
5. Short taps ≈ break-in CW; long hold ≈ continuous / SSB.
6. **q** or **Esc** → **release hold** and quit.

Default target: `127.0.0.1:22372`. Detail: [docs/TX_INHIBIT.md §6](docs/TX_INHIBIT.md#6-testing-locally).

### Python — `tools/send_inhibit_hold.py`

From a git clone (stdlib only — no pip packages):

```bash
# Linux / macOS (Linux --global style needs /dev/input for grave KEY)
python3 tools/send_inhibit_hold.py --interactive

# Windows (if "python" is on PATH)
python tools\send_inhibit_hold.py --interactive
```

Same **level** Spacebar behaviour as `inhibit-spacebar`. Optional flags:

```bash
python3 tools/send_inhibit_hold.py -i --station SSB-TEST --host 127.0.0.1 --port 22372
```

| Flag | Purpose |
|------|---------|
| `-i` / `--interactive` | Spacebar KEY **level** + hang |
| `--station NAME` | Text shown in the badge (`held by …`) |
| `--host` / `--port` | WSJT-X station address (default `127.0.0.1:22372`) |
| `--fixed-hang-ms N` | Fixed hang after KEY up (disable adaptive) |

One-shot hold (no Spacebar loop):

```bash
python3 tools/send_inhibit_hold.py --ttl-ms 3000 --station TEST   # hold ~3 s
python3 tools/send_inhibit_hold.py --ttl-ms 0                     # release
```

Script on GitHub: [tools/send_inhibit_hold.py](https://github.com/wa1hco/wsjtx-inhibit/blob/main/tools/send_inhibit_hold.py).

### Production use

Day-to-day multi-op use is a **KEY agent**: senses the priority KEY and tells WSJT-X stations not to transmit (UDP keepalives + **release hold**; see [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md) §3). Spacebar helpers are **bench stand-ins**.

---

## 7. Problems and feedback

Please include:

- Installer / SmartScreen / antivirus issues  
- OS, rig, and **PTT method + PTT port** (exact `COMx` / `/dev/tty…`)  
- Whether normal FT8 receive/TX worked  
- Whether the red **TX INHIBITED** badge appeared and whether the radio keyed or not  

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
