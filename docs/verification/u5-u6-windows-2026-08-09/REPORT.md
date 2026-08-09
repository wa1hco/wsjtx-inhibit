# U5/U6 verification — Windows run 2026-08-09

## Environment

| Item | Value |
|------|--------|
| Host | DESKTOP-RTM3TE1 |
| Windows | 10.0.26200.7171 |
| COM ports | COM1 (ACPI motherboard UART only; no USB-serial adapter) |
| `rigctl` | Hamlib **4.7.1** `2026-04-15T20:20:01Z` SHA=`d042479` 64-bit (`C:\WSJT\wsjtx\bin\rigctl-wsjtx.exe`) |
| Hamlib source | `C:\Users\jeff\Documents\Hamlib-4.7.1` |
| Probe tool | `u5u6-hamlib-test\check_com_probe.exe` |

## Step 0 — version

In range 4.6.1–4.7.2 for the code-path analysis: **YES** (4.7.1).

## Step 3 — stock Hamlib: bare vs `\\.\` path

**Note:** Dummy model (`-m 1`) does **not** open the serial port; it cannot test this.

Used: `rigctl-wsjtx.exe -m 3073` (IC-7300) `-r <path> -vvvvv f`

| Port arg | `serial_open` | Notes |
|----------|---------------|-------|
| `COM1` | **OK** — `serial_open: serial port COM1 is OK` | No radio; CI-V timeouts after open are expected |
| `\\.\COM1` | **OK** — `serial_open: serial port \\.\COM1 is OK` | **Does not fail** — review expected failure |

## CreateFile probe (exact Hamlib `check_com_port_in_use` snprintf)

Unguarded formula: `snprintf(device, "\\\\.\\%s", port)`

| Input `port` | Built `device` | CreateFileA |
|--------------|----------------|-------------|
| `COM1` | `\\.\COM1` | OK |
| `\\.\COM1` | `\\.\\\.\COM1` (double prefix) | **OK** on this OS |
| `COM99` | `\\.\COM99` | FAIL err=2 (no such port) |
| `\\.\COM99` | `\\.\\\.\COM99` | FAIL err=2 |

Guarded (proposed patch) and unguarded both open COM1 successfully.

**Conclusion:** The non-idempotent prefix is real in Hamlib source, but on this Windows build the doubled path still opens. The review’s “CreateFile fails → `-RIG_EIO` / does not exist” outcome **did not reproduce**.

Caveat: only motherboard COM1 was available. USB-CDC / FTDI drivers might treat weird device paths more strictly; re-test if a USB CAT cable is available.

## Step 4 — patched Hamlib rebuild

Skipped. Not needed to disprove the hard-failure claim (unguarded already succeeds). The patch remains a reasonable **idempotency** cleanup for Hamlib, not a proven crash/open fix on this host.

## U5 (WSJT-X DO-NOT-APPLY CAT prefix)

Not run (no WSJT-X rebuild). Given extended paths open here, the claim “breaks every COM port” is **not confirmed** on this machine. Skip shipping that WSJT-X hunk still looks right (redundant / depends on Hamlib); the catastrophic-failure narrative needs evidence on a machine where double-prefix fails.

## Report line for REVIEW-rc2

- `rigctl -V`: Hamlib 4.7.1 2026-04-15T20:20:01Z SHA=d042479 64-bit  
- Step 3 bare `COM1`: open **OK** (expected)  
- Step 3 prefixed `\\.\COM1`: open **OK** — **did not match** expected failure  
- **Negative result** for the hard-fail claim on this host  

## Artefacts

- `REPORT.md` (this file)  
- `probe-results.txt`  
- `check_com_probe.c` / `check_com_probe.exe`  
