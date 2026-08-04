# TX Inhibit spacebar tester (GUI)

For **Windows testers** (and Linux) who need to exercise **wsjtx-inhibit**
without remembering CLI flags.

## Quick start (Windows)

1. Install [Python 3](https://www.python.org/downloads/) if needed  
   (check **tcl/tk** / tkinter during setup).
2. Double-click **`Run-InhibitSpacebar.cmd`**  
   or: `py -3 tools\inhibit_spacebar_gui.py`
3. Start **wsjtx-inhibit** with **PTT method = RTS or DTR** on a COM port.
4. Click the window, then **hold SPACE** (or hold the green button).
5. WSJT-X status bar should show **TX INHIBITED — held by …** while space is down.

## What it does

| Action | Packet |
|--------|--------|
| Space / button **down** | Hold: `ttl_ms` (default 600) + keepalives every 200 ms |
| Space / button **up** | Release: `ttl_ms: 0` |
| Escape / window close | Force release |

Same JSON protocol as `send_inhibit_hold.py` and the in-app gate:

```json
{"tx_inhibit":1,"ttl_ms":600,"station":"TEST-SSB","band":"144","seq":1}
```

## GUI fields

| Field | Meaning |
|-------|---------|
| Host / Port | Gate address (default `127.0.0.1:22372`) |
| Station | Shown in WSJT-X badge (“held by …”) |
| Band | Optional band tag in the datagram |
| TTL ms | Deadman window (100–30000); keepalives re-arm it |
| Keepalive ms | Re-send period while held (default 200) |

Status panel shows last packet, counters, and estimated hold deadline.

## CLI (still available)

```bash
py -3 tools/send_inhibit_hold.py --ttl-ms 2000 --station TEST
py -3 tools/send_inhibit_hold.py --ttl-ms 0
```

## Linux

```bash
sudo apt install python3-tk   # if needed
python3 tools/inhibit_spacebar_gui.py
```
