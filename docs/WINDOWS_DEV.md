# Windows development notes — wsjtx-inhibit

## Transfer to lab VM (Tiny11)

From the Linux host (HTTP on `192.168.122.1:8765`):

```powershell
# Elevated PowerShell on guest
iex (irm http://192.168.122.1:8765/setup-inhibit-win.ps1)
```

Source lands at **`C:\src\wsjtx-inhibit`**. Build notes: `C:\WIMS-lab\INHIBIT-WINDOWS-BUILD.txt`.

Or clone:

```powershell
git clone https://github.com/wa1hco/wsjtx-inhibit.git C:\src\wsjtx-inhibit
```

## Build (MinGW + Qt5)

Upstream expects the [Hamlib SDK](https://sourceforge.net/projects/hamlib-sdk/) (MinGW, FFTW, Boost, etc.) or an equivalent toolchain. See also `doc/building on MS Windows.txt` in the tree if present.

Helper script (after Qt MinGW is installed under `C:\Qt`):

```powershell
powershell -ExecutionPolicy Bypass -File C:\src\wsjtx-inhibit\scripts\windows\Build-Inhibit.ps1
```

Install prefix default: **`C:\WSJT\wsjtx-inhibit`**.  
Optional portable zip: **`C:\WSJT\wsjtx-inhibit-win64-portable.zip`**.

NSIS installer: configure CMake as usual; on Windows `CPACK_GENERATOR` includes **NSIS** (`CMakeLists.txt`). From the build dir:

```bat
cpack -G NSIS
```

## TX Inhibit on Windows (COM / RTS / DTR)

Architecture (platform-neutral):

- **`TxInhibitGate`** — UDP KEY agent listen; mixes hold with software TX intent.
- **`HamlibTransceiver::do_ptt`** — when inhibit is enabled, intent goes to the gate; **`apply_physical_ptt`** drives Hamlib pins (RTS/DTR/CAT).
- **No serial open in the gate** — Hamlib owns the COM port.

Windows-specific attention:

| Area | Notes |
|------|--------|
| **PTT method** | RTS or DTR only for inhibit tests (not CAT-as-PTT method). |
| **COM path** | Hamlib wants `COMn` or `\\.\COMn` for ports ≥10 — verify open failures on USB serial. |
| **Handle inheritance** | `NonInheritingProcess` (Win32 `STARTUPINFOEX`) used so child processes do not steal COM handles. |
| **Shared CAT+PTT USB** | Handshake None; only one app owns modem lines. |
| **Helpers** | `inhibit-test.exe` next to `wsjtx.exe`. |

### Candidate Windows hardening (implement as needed)

1. Normalize PTT port strings to `\\.\COMx` when `x >= 10` before Hamlib open.
2. Log `GetLastError` on pin set failure for clearer UI errors.
3. Ensure UAC / non-admin can open user COM devices (no admin requirement for normal PTT).
4. Package `inhibit-test*.exe` into NSIS/zip alongside `bin\wsjtx.exe`.

## Operator install (no compile)

See [INSTALL-WINDOWS.md](../INSTALL-WINDOWS.md) and GitHub **Releases** assets (`-win64.exe` / portable zip).
