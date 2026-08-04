# Install on Windows — wsjtx-inhibit

**Audience: operators and testers.**  
Step-by-step install of the Windows **wsjtx-inhibit** program (WSJT-X + TX Inhibit).

You do **not** need Microsoft Visual Studio, MSYS2, or anything called “NSIS.”

---

## Where do I get the program?

1. Open **[Releases](https://github.com/wa1hco/wsjtx-inhibit/releases)**.
2. Open the release you were asked to test (for example **[wsjtx-inhibit-rc1](https://github.com/wa1hco/wsjtx-inhibit/releases/tag/wsjtx-inhibit-rc1)**).
3. Use the download links in the release notes, or expand **Assets** if needed (same files).

### Which file do I download?

| File | Meaning | What you do |
|------|---------|-------------|
| **`…-win64.exe`** (e.g. `wsjtx-inhibit-rc1-win64.exe`) | Windows **installer** (recommended) | Download → double-click → Next, Next, Finish |
| **`…-windows-x86_64.zip`** (e.g. `wsjtx-inhibit-rc1-windows-x86_64.zip`) | **Portable** copy (no installer) | Download → unzip → run `bin\wsjtx.exe` |

The **`.exe`** is the setup program itself (built with a packaging tool on the maintainers’ side). Treat it like any other Windows installer.

### There is no Windows package on that release?

Use another release that lists a `-win64.exe` or Windows `.zip`, or contact the person who sent you the link.

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
   - From the **Start** menu (shortcut the installer created),  
   - From a **desktop** shortcut if the installer offered one, or  
   - Open the install folder → **`bin`** → double-click **`wsjtx.exe`**.

### What the installer typically creates

| Item | Result |
|------|--------|
| Program files | Under `C:\WSJT\…` (path is choosable in the wizard) |
| Start menu | Launch shortcut, documentation/site links, Uninstall |
| Desktop | Optional app shortcut when that option is enabled |
| Finish page | Option to launch the app when setup completes |
| Windows apps list | Uninstaller entry for this install |

### Can I keep official WSJT-X installed?

Yes. Prefer side-by-side installs. For testing, always launch **this** build’s Start-menu entry or `wsjtx.exe`. Use one program at a time on a given radio and COM ports.

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

Inhibit only gates the **physical serial PTT key line** (RTS or DTR). It does **not** stop CAT-only PTT, and it is **not** the same as **Halt Tx**.

### Required radio settings

1. **File → Settings → Radio**
2. **PTT method** = **RTS** or **DTR**  
   - Do **not** use **CAT** as the PTT *method* for inhibit tests.  
   - Do **not** rely on **VOX** alone.
3. **PTT port** = a real **`COMx`** (not the special list value **CAT**).  
   - **Same COM as CAT** is valid and common (radio USB CAT + RTS/DTR).  
   - A **separate** PTT COM is also fine if you use an external keyline adapter.
4. Wire **RTS** or **DTR** to the radio’s PTT / SEND, or use the radio’s **USB SEND** / **PC KEYING** map on that COM.
5. On the radio, turn **VOX off** for the test so only the key line can key the transmitter.
6. **Shared USB CAT + RTS/DTR checklist:** **Handshake = None**; radio menu assigns the line to SEND/PTT (not flow control); only one program drives the modem lines. Details: [docs/TX_INHIBIT.md — Shared USB CAT + RTS/DTR](docs/TX_INHIBIT.md#shared-usb-cat--rtsdtr-what-operators-actually-do).

### What you should see when held

- A red status-bar badge: **`TX INHIBITED`** or **`TX INHIBITED — held by …`**.
- The radio **must not key** (no RF) even if WSJT-X is in a TX cycle and audio is still playing.

Default inhibit UDP port is **22372** (if free). Design (gate + KEY agent): [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).

### Optional local inhibit test

With this build running and PTT set as above, use the helper **shipped next to the app**:

```text
bin\wsjtx.exe
bin\inhibit-spacebar.exe
```

1. Run **`bin\inhibit-spacebar.exe`** (same install folder as `wsjtx.exe`).
2. **Hold Space** → red **TX INHIBITED** badge; attempt TX (dummy load) — radio stays unkeyed.
3. **Release Space** → after hang, badge clears; normal PTT works. Short taps simulate CW (adaptive hang).
4. **q** or **Esc** to quit the helper. Space follows KEY **level** (down while pressed).

Default UDP target is `127.0.0.1:22372`. Day-to-day multi-op uses a real **KEY agent**; this helper is a bench stand-in. See [docs/TX_INHIBIT.md](docs/TX_INHIBIT.md).

---

## How do I know I have the right program?

1. You started **this** install’s `bin\wsjtx.exe` (or its Start-menu shortcut), not stock WSJT-X from SourceForge/ARRL.
2. **Help → About** shows **wsjtx-inhibit** and base version **3.0.x** (mainline + TX Inhibit). The main window title starts with **wsjtx-inhibit**.
3. Normal FT8/FT4 decode works; with RTS/DTR PTT, TX keys the radio when **not** inhibited.
4. During a hold (KEY agent or the smoke test above), the red **TX INHIBITED** badge appears and the radio does not key.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| SmartScreen blocks the `.exe` | More info → Run anyway (if you trust this repo) |
| Antivirus quarantines the file | Restore from quarantine or add an exception for the download; report false positive if needed |
| Program starts but no radio control | Same as stock WSJT-X: CAT port, baud, driver, cable |
| Error about opening a COM port / “TX Inhibit: cannot open …” | Wrong `COMx`, permissions, or another program already owns the port — close the other app or pick the correct COM |
| PTT never keys at all after install | Check **PTT method** is RTS/DTR and **PTT port** is a real `COMx` (**not** the special value “CAT”). Confirm USB SEND / PC KEYING and wiring |
| Radio keys when the program opens the COM | DTR/RTS polarity or another app forcing the line — see [shared USB CAT + RTS/DTR](docs/TX_INHIBIT.md#shared-usb-cat--rtsdtr-what-operators-actually-do) |
| CAT flaky after enabling RTS PTT | Handshake must be **None**; radio must not use RTS for CAT flow control |
| Inhibit never holds TX | Need RTS/DTR on a real serial PTT port; **CAT-only PTT is not gated**. Confirm hold packets reach port **22372** (or the port your interlock was told) |
| Radio still keys during inhibit | Radio **VOX** may still key from audio — turn VOX off; confirm you are not using a second PTT path |
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
