# Install on Windows — wsjtx-inhibit

**Audience: operators and testers.**  
Step-by-step install of the Windows **wsjtx-inhibit** program (WSJT-X + TX Inhibit).

You do **not** need Microsoft Visual Studio, MSYS2, or anything called “NSIS.”

---

## Where do I get the program?

1. Open **[Releases](https://github.com/wa1hco/wsjtx-inhibit/releases)**.
2. Open the release you were asked to test (for example *Windows test build 3.0.2*).
3. Scroll to **Assets** (file list at the bottom).

### Which file do I download?

| Under Assets, look for | Meaning | What you do |
|------------------------|---------|-------------|
| File ending in **`-win64.exe`** | Windows **installer** (recommended) | Download → double-click → Next, Next, Finish |
| File ending in **`.zip`** with `windows` in the name | **Portable** copy (no installer) | Download → unzip → run `bin\wsjtx.exe` |

Example names (exact names change with version):

```text
wsjtx-…-win64.exe              ← use this if you want a normal install
wsjtx-…-windows-x86_64.zip     ← use this for a portable folder
```

Names may still say `mainline-wims`. That is still this project.

### What about “NSIS”?

**You do not install NSIS.**  
NSIS is a tool *package builders* use to create a Windows setup program. The file you download that ends in **`.exe`** *is already* that setup program. Treat it like any other Windows installer.

### There is no .exe in Assets?

Then that release has no Windows package. Use another release that lists a `-win64.exe` or Windows `.zip`, or contact the person who sent you the link.

---

## Method A — installer (recommended)

1. **Close** stock WSJT-X and any older wsjtx-inhibit windows.
2. Download the **`-win64.exe`** from **Assets**.
3. Double-click the downloaded file.
4. If **User Account Control** asks for permission, choose **Yes**.
5. If **Windows protected your PC** (SmartScreen) appears:
   - Click **More info**
   - Click **Run anyway**  
   - Only do this if you trust **this** GitHub project (`wa1hco/wsjtx-inhibit`).
6. Follow the installer screens.  
   Default location is often under **`C:\WSJT\`** (for example `C:\WSJT\wsjtx` or similar).
7. When finished, start the program:
   - From the **Start** menu (search for WSJT-X / the name the installer created), or  
   - Open the install folder → **`bin`** → double-click **`wsjtx.exe`**.

### Can I keep official WSJT-X installed?

Yes. Prefer side-by-side installs. For testing, always launch **this** build’s `wsjtx.exe`, not the SourceForge/ARRL one. Do not run both against the same radio and COM ports at the same time.

---

## Method B — portable ZIP

1. Download the Windows **`.zip`** from **Assets**.
2. Right-click → **Extract All…** (or use 7-Zip).  
   Example folder: `C:\WSJT\wsjtx-inhibit\`
3. Open the extracted folder.
4. Open **`bin`**.
5. Double-click **`wsjtx.exe`**.

Optional: right-click `wsjtx.exe` → **Send to** → **Desktop (create shortcut)**.

---

## First-time settings (same as normal WSJT-X)

1. **File → Settings → General** — callsign, grid square, etc.
2. **Settings → Radio** — your transceiver, CAT port, baud rate.
3. **Settings → Audio** — sound input and output devices.
4. **Settings → Reporting** — optional (PSK Reporter, etc.).

Click **OK**. Try receive first, then a careful TX test (dummy load recommended).

---

## Settings for TX Inhibit testing

Inhibit only controls the **serial PTT key line** in this build.

1. **File → Settings → Radio**
2. Set **PTT method** to **RTS** or **DTR** (not “CAT” alone, not VOX-only for this test).
3. Select the **COM port** of your USB-serial PTT adapter (often a **different** port than CAT).
4. Connect that adapter’s RTS or DTR line to the radio’s PTT / SEND as you would for any digital-mode interface.

When inhibit is active, the status area may show something like **TX INHIBITED**.  
The radio should **not** key even if WSJT-X is in a TX cycle.

More detail: [docs/WIMS_TX_INHIBIT.md](docs/WIMS_TX_INHIBIT.md).

---

## How do I know I have the right program?

1. You started the copy you just installed (not the old Start-menu stock WSJT-X by mistake).
2. **Help → About** shows version **3.0.x** matching the release you downloaded.
3. Normal FT8/FT4 decode and (with correct settings) transmit work as with stock WSJT-X.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| SmartScreen blocks the `.exe` | More info → Run anyway (if you trust this repo) |
| Antivirus quarantines the file | Restore from quarantine or add an exception for the download; report false positive if needed |
| Program starts but no radio control | Same as stock WSJT-X: CAT port, baud, driver, cable |
| Inhibit never holds TX | PTT must be **RTS or DTR** on the inhibit-capable path; CAT-PTT-only is not gated |
| “I can’t find Linux files here” | Correct — this page is Windows only. See [INSTALL.md](INSTALL.md) |

---

## Feedback

Please report install issues, Windows version, rig, PTT method, and whether inhibit worked:

[https://github.com/wa1hco/wsjtx-inhibit/issues](https://github.com/wa1hco/wsjtx-inhibit/issues)

---

## Maintainers only (not for operators)

How Windows packages are produced and published:

- Preferred operator path: GitHub Release **Assets** (`.exe` installer + portable `.zip`).
- CI / tags: see [INSTALL.md](INSTALL.md) maintainer section and `.github/workflows/`.
- Local MSYS2 stage builds: [docs/BUILDING.md](docs/BUILDING.md), `scripts/windows/`.
- Product should be launched from **this** install’s `bin\wsjtx.exe` so the inhibit code is present.

Typical multi-seat layout:

```text
C:\WSJT\wsjtx-inhibit\
  bin\wsjtx.exe
  bin\jt9.exe
  ...
```

Not an official WSJT-X release. GPL-3.
