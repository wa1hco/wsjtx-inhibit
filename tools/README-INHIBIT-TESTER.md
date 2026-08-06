# KEY agent stand-ins (TX Inhibit)

**Design authority:** [docs/TX_INHIBIT.md §3](../docs/TX_INHIBIT.md)  
(Hold sender + KEYing monitor, hang vs hold timeout, race rules)

## Program name

| Name | Role |
|------|------|
| **`inhibit-spacebar`** | **Canonical.** Cross-platform console KEY agent; installed as `bin/inhibit-spacebar` next to `wsjtx`. |
| `inhibit_spacebar` | Windows-only GUI (underscore); same protocol; optional alternate binary. |
| `send_inhibit_hold.py` | Python stand-in (dev / scripted tests). |

Prefer **`inhibit-spacebar`** in docs and scripts.

## Build / install

```text
tools/inhibit-spacebar/main.cpp  →  target inhibit-spacebar  →  bin/inhibit-spacebar
tools/inhibit_spacebar/          →  target inhibit_spacebar  →  bin/inhibit_spacebar.exe  (Windows GUI)
```

## Behaviour (canonical tool)

**KEY key = grave/backtick `` ` ``** (not Space — avoids false holds while typing).

1. **KEY assert** (`` ` `` down) → **hold** immediately (`ttl_ms` = hold_timeout_ms, default 600) + keepalives ~200 ms.  
2. **KEYing monitor** classifies break-in CW vs continuous KEY; measures dit if break-in.  
3. **KEY open** → hang:  
   - **Break-in CW:** hang = **1.5 × word gap** (= 10.5 × dit), clamp ~315–1260 ms (≈40–10 WPM).  
   - **Continuous** (long mark / SSB / non-break-in): hang = **0** → **release hold** immediately.  
4. Hang done → **stop keepalives**, then **`ttl_ms: 0`** (no race).  

```bash
inhibit-spacebar --host 127.0.0.1 --port 22372 --station TEST-KEY --ttl-ms 600
inhibit-spacebar --fixed-hang-ms 0    # force hang 0 (continuous-style)
```

Requires **Enable TX Inhibit** and RTS/DTR on the WSJT-X station under test.
