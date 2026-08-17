# rc2 Windows verification checklist

Everything still unverified for rc2, in priority order. Self-contained: no scratch
files, no context from the review conversation needed.

**Each step states its expected result and what a contrary result means.** A contrary
result is a finding, not a failure — report it rather than working around it.

Record answers inline and commit this file back, or paste the results.

| Env | Value |
|---|---|
| Windows version | |
| Build dir used | |
| Date | |

---

## 0. Get the code

```bat
cd C:\src\wsjtx-inhibit
git pull
```

Expected HEAD: `7e76fae feat(inhibit): show TX Inhibit state in the existing status label`
(or later).

---

## 1. R3 — where does the installer actually put things? **(last open blocker)**

The whole point of the rc2 packaging work: this build must not land in
`C:\WSJT\wsjtx`, where official WSJT-X installs.

Build, then from the build directory:

```bat
cpack -G NSIS
findstr /C:"InstallDir" _CPack_Packages\win64\NSIS\project.nsi
```

**Expected:** `InstallDir "C:\WSJT\wsjtx-inhibit"`

- Still says `C:\WSJT\wsjtx` → R3 is **not** fixed; the CPack override is not reaching
  the NSIS generator. Report the line verbatim.

Then check the two identity strings that decide how it appears in Add/Remove Programs:

```bat
findstr /C:"DisplayName" /C:"UninstallString" _CPack_Packages\win64\NSIS\project.nsi
findstr /C:"!define MUI_" _CPack_Packages\win64\NSIS\project.nsi | findstr /I product
```

**Expected:** the display name contains **wsjtx-inhibit**, *not* the stock string
"WSJT-X: Digital Modes for Weak Signal Communications in Amateur Radio".

**Also record the produced filename:**

```bat
dir *.exe
```

**Expected:** `wsjtx-inhibit-<version>-win64.exe`. If it is `wsjtx-<version>-win64.exe`,
the CI filename override and the CMake default disagree — report it (that is R5).

| Check | Result |
|---|---|
| `InstallDir` line | |
| Display name | |
| Installer filename | |

---

## 2. R4 — does `--rig-name` really isolate settings?

Verified on Linux; the code is platform-independent Qt, so this is a confirmation.

Launch the built app **twice**, separately:

```bat
wsjtx.exe
wsjtx.exe --rig-name inhibit
```

Quit each cleanly, then:

```bat
dir "%LOCALAPPDATA%\WSJT-X*"
dir "%APPDATA%\WSJT-X*"
```

**Expected:** a plain `WSJT-X.ini` **and** a separate `WSJT-X - inhibit.ini`.

- Only one file → `--rig-name` is not isolating, and the R4 mitigation documented in
  INSTALL-WINDOWS.md is wrong. Important — say so.

---

## 3. Does the gate bind at all, and does Windows Firewall interfere?

You do not need a radio. With **Rig = None** and **PTT method = RTS** on a real COM
port (motherboard `COM1` is fine), the PTT-only path still starts the inhibit gate.

1. Settings → Radio: **Rig** = None, **PTT method** = RTS, **PTT port** = `COM1`,
   **Enable TX Inhibit** = checked. OK.
2. Note whether a **Windows Defender Firewall** prompt appears.

```bat
netstat -ano -p UDP | findstr 22372
```

**Expected:** a UDP listener on `22372` owned by the wsjtx.exe PID.

- Nothing on 22372 → either the gate did not start (PTT method not RTS/DTR, or the COM
  port failed to open) or it fell back to an ephemeral port. Hover the
  `Receiving` / `Tx:` status box — the tooltip should say which.

| Check | Result |
|---|---|
| Firewall prompt appeared? | |
| UDP 22372 listening? | |
| Tooltip text | |

---

## 4. The inhibit indicator — the new UI

Hold from a **separate PowerShell window** so WSJT-X keeps keyboard focus. The
interactive helper (`inhibit-test`) reads a held key and therefore
needs focus itself, which makes it impossible to hold the KEY *and* press Tune at
the same time. Use **`inhibit-agent`** for a real CTS KEY (no keyboard focus).

```powershell
cd C:\src\wsjtx-inhibit\tools
.\Send-InhibitHold.ps1              # holds until Ctrl+C, with keepalives
.\Send-InhibitHold.ps1 -Seconds 20  # holds 20 s, then auto-releases
.\Send-InhibitHold.ps1 -Release     # clear a stuck hold
```

If PowerShell blocks the script: `powershell -ExecutionPolicy Bypass -File .\Send-InhibitHold.ps1`

One-liner equivalent, if you would rather not run a script — a single hold lasts at
most 30 s (protocol maximum for `ttl_ms`):

```powershell
$u=[System.Net.Sockets.UdpClient]::new()
$b=[Text.Encoding]::UTF8.GetBytes('{"tx_inhibit":1,"ttl_ms":30000,"station":"TEST"}')
$u.Send($b,$b.Length,"127.0.0.1",22372)
```

**4a — while receiving** (Monitor on, not transmitting)

**Expected:** the left status box changes from green `Receiving` to **pale green
`Inhibit`**, and reverts after ~8 s.

**4b — while transmitting or tuning**

**Press Tune first, then apply the hold.** Tune is a latching toggle, so it stays on
while you switch windows — no timing pressure and no fighting over focus:

1. In WSJT-X press **Tune**. Box shows yellow `Tx: TUNE`.
2. Switch to the PowerShell window and run `.\Send-InhibitHold.ps1`.
3. Look back at WSJT-X **without clicking on it** (the label updates regardless of
   focus; clicking is fine too, it just is not needed).
4. Ctrl+C in PowerShell to release, and confirm the box returns to yellow `Tx: TUNE`.
5. Press **Tune** again in WSJT-X to stop tuning.

**Expected at step 3:** the box shows **red `Inhibit`**, *not* yellow `Tx: TUNE`.
**Expected at step 4:** it returns to yellow `Tx: TUNE` — the hold released, but Tune
is still on.

Rig = None will not key a real radio, but RTS on COM1 does toggle, so this exercises
the actual pin path.

This is the case an earlier build got wrong: the override was in the receive branch
only, so Tune showed yellow while the radio really was held. It is the single most
valuable check on this page after step 1.

**4c — no second box.** Confirm there is no extra status-bar widget and the status bar
spacing is unchanged from stock.

| Check | Result |
|---|---|
| 4a pale green `Inhibit` while receiving | |
| 4b red `Inhibit` during Tune | |
| 4b returns to yellow `Tx: TUNE` on release | |
| 4c no extra box, spacing unchanged | |

---

## 5. C4 — the "you are not protected" warning

Occupy the well-known port *before* starting WSJT-X, and see whether the app notices:

```powershell
# leave this window open
$listener=[System.Net.Sockets.UdpClient]::new(22372)
```

Start WSJT-X with TX Inhibit enabled as in step 3.

**Expected (uncertain — this is the interesting part):** a one-shot status-bar message
along the lines of *"TX Inhibit: listening on <N>, not 22372 — a KEY agent aimed at
22372 will not reach this station"*, and the tooltip saying the same.

**Genuinely unknown:** the gate binds with `ShareAddress | ReuseAddressHint`. On
Windows `SO_REUSEADDR` historically permits binding a port already in use, so WSJT-X
may bind 22372 successfully *anyway* and never fall back. If so:

- the warning will not appear (correct — it did bind 22372), **and**
- which process receives a given datagram is decided by Windows.

Either outcome is worth knowing. Report which happened, and if both bound, whether the
hold from step 4 still reached WSJT-X.

| Check | Result |
|---|---|
| Did WSJT-X bind 22372 anyway? | |
| Warning message seen? | |
| Did holds still arrive? | |

---

## 6. Side-by-side install (optional, needs a throwaway VM snapshot)

Only if you can snapshot/roll back. Install official WSJT-X first, then this build's
`.exe`.

**Expected:** two separate directories (`C:\WSJT\wsjtx` and `C:\WSJT\wsjtx-inhibit`),
two distinguishable Add/Remove Programs entries, and uninstalling one leaves the other
working.

---

## 7. Loose end — the original COM-port symptom

U5/U6 concluded the `\\.\COM` change was inert: Hamlib already adds the prefix
idempotently, and Windows normalizes the doubled form. So whatever originally prompted
that change is **still unexplained**.

If you remember the symptom — a rig that would not open, a specific COM number, an
error string — write it here. There may be a real bug behind it that the inert fix
masked.

```
(symptom, if recalled)
```

---

## Reporting

Fill in the tables, commit this file, push. Or paste the results back.

Most valuable single answer: **step 1's `InstallDir` line.** It is the last thing
standing between here and tagging rc2.
