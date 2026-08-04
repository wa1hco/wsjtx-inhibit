# wsjtx-inhibit-rc1 — Windows

**Who this is for:** operators who already use WSJT-X and want to try **TX Inhibit**.

**What this is:** a branch of **WSJT-X mainline v3.0.2** with a **low-latency TX Inhibit** function. Same digital modes and sequencing as mainline; the radio PTT key line can be held off when the multi-op interlock requires it.

Multi-op and same-band stations must keep **only one station transmitting at a time**. Inhibit lets you **interleave WSJT-X contacts with SSB/CW** efficiently: when interlock software (or a test tool) says the band is busy, this build holds **PTT to the WSJT-X radio** within milliseconds while FT8/FT4 sequencing and audio continue as usual. That is separate from **Halt Tx**, which ends the QSO sequence.

This is an independent GPL-3 build (same license family as WSJT-X). It is a community/test release candidate, separate from official ARRL / WSJT Development Group packages.

---

## Download (Windows 64-bit) — pick one

| Link | Type | What you get |
|------|------|----------------|
| **[wsjtx-inhibit-rc1-win64.exe](https://github.com/wa1hco/wsjtx-inhibit/releases/download/wsjtx-inhibit-rc1/wsjtx-inhibit-rc1-win64.exe)** | **Installer** (recommended) | Normal Windows setup |
| **[wsjtx-inhibit-rc1-windows-x86_64.zip](https://github.com/wa1hco/wsjtx-inhibit/releases/download/wsjtx-inhibit-rc1/wsjtx-inhibit-rc1-windows-x86_64.zip)** | **Portable ZIP** | Unzip and run; no installer |

These are the only packages for this release. Expand **Assets** on this page if you prefer the GitHub file list — same two files, short names ending in **`.exe`** (installer) or **`.zip`** (portable).

---

## Install — installer (recommended)

1. Close any running WSJT-X windows.
2. Download **`wsjtx-inhibit-rc1-win64.exe`** (table above).
3. Run the installer.
   - Windows may show **SmartScreen** (“Windows protected your PC”) on an unsigned test build.
   - **More info** → **Run anyway** if you trust this GitHub project (`wa1hco/wsjtx-inhibit`).
4. Complete the wizard. Default install root is typically under **`C:\WSJT\`** (for example `C:\WSJT\wsjtx\`).
5. Start the program from the **Start menu**, desktop shortcut (if created), or `bin\wsjtx.exe` in the install folder.

### What the installer sets up

| Item | Typical result |
|------|----------------|
| Program files | Under `C:\WSJT\…` (you can change the path in the wizard) |
| **Start menu** | Shortcut to launch the app, plus links such as documentation / project site, and **Uninstall** |
| **Desktop** | Optional desktop shortcut for the app (when that option is enabled) |
| **Finish page** | Option to launch the app when setup completes |
| **Add/Remove Programs** | Uninstaller entry for this install |

You can keep official WSJT-X installed beside this build. For inhibit testing, launch **this** install’s Start-menu entry or `bin\wsjtx.exe`. Use one program at a time on a given radio and COM ports.

---

## Install — portable ZIP

1. Download **`wsjtx-inhibit-rc1-windows-x86_64.zip`**.
2. Extract to a folder you choose, for example `C:\WSJT\wsjtx-inhibit\`.
3. Open that folder → **`bin`** → double-click **`wsjtx.exe`**.

Optional: right-click `wsjtx.exe` → **Send to** → **Desktop (create shortcut)**.

---

## First launch (same settings as mainline WSJT-X)

1. **File → Settings → General** — callsign, grid, etc.
2. **Settings → Radio** — rig, CAT port, baud rate.
3. **Settings → Audio** — sound input/output.
4. **Settings → Reporting** — optional (PSK Reporter, etc.).

### Settings for TX Inhibit

Inhibit gates the **serial PTT key line**. Set:

1. **File → Settings → Radio**
2. **PTT method** = **RTS** or **DTR**
3. **PTT port** = a real **`COMx`** (not the special list value “CAT”). Same COM as CAT is fine for radio USB RTS/DTR; a separate PTT adapter is also fine.
4. Wire RTS or DTR to PTT/SEND, or use the radio’s USB SEND / PC KEYING map; **Handshake = None**
5. Leave radio **VOX** off so the key line alone controls PTT

When a hold is active, the status bar shows a red badge such as **TX INHIBITED** (or **held by …**). The radio stays unkeyed while WSJT-X may still be in a TX cycle in software.

Default inhibit UDP port: **22372**.

More detail: [docs/TX_INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md)

---

## How inhibit is triggered

Inhibit engages when this program **receives an inhibit datagram** (UDP packet) on its inhibit port. Local CTS KEY sensing is not used in this build.

Sources include:

| Source | Role |
|--------|------|
| **KEY agent** | Program that senses the priority radio’s KEY line and sends hold/keepalive/release UDP (see [docs/TX_INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md)) |
| **Inhibit test helper** | Bench stand-in: `bin\inhibit-spacebar.exe` or `tools/send_inhibit_hold.py` |

### Quick check with a test hold

1. Run this build with **PTT = RTS or DTR** as above.
2. Have a KEY agent (or the test helper) send a **hold** datagram to port **22372**.
3. Attempt TX (dummy load recommended) — the radio stays unkeyed; the red badge appears.
4. On **release** (or when the hold TTL expires), PTT works again.

---

## Confirm you have this build

1. Launch the copy you just installed (Start menu / desktop / portable `bin\wsjtx.exe`).
2. **Help → About** shows **wsjtx-inhibit** and base version **3.0.2** (mainline + TX Inhibit). The main window title starts with **wsjtx-inhibit**.
3. Receive and TX behave like mainline when open (no hold).
4. During a hold, the red **TX INHIBITED** badge appears and the radio stays unkeyed.

---

## Feedback

Please tell us:

- Installer or ZIP experience (including SmartScreen / antivirus)
- Windows version
- Rig and PTT setup (RTS/DTR `COMx`)
- Whether FT8 behaved like mainline
- Whether inhibit held and released cleanly
- Anything on this page that was unclear

Issues: https://github.com/wa1hco/wsjtx-inhibit/issues

---

## Safety and license

- Prefer a **dummy load** or carefully controlled power for first TX tests.
- This is **rc1** — a release candidate for operator testing.
- Prefer installing **beside** official WSJT-X until you are happy with it.
- **GPL-3**, same family as WSJT-X. Separate from the official WSJT project.

---

## More documentation

| Link | Topic |
|------|--------|
| [INSTALL.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL.md) | Install overview |
| [INSTALL-WINDOWS.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/INSTALL-WINDOWS.md) | Windows step-by-step |
| [README.INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/README.INHIBIT.md) | Project overview |
| [docs/TX_INHIBIT.md](https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/TX_INHIBIT.md) | Gate + KEY agent design and wiring |
