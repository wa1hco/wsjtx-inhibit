# KEY agent stand-ins (TX Inhibit)

**Design authority:** [docs/TX_INHIBIT.md §3](../docs/TX_INHIBIT.md)  
(Hold sender + KEYing monitor, hang vs hold timeout, race rules)

## Program names

| Name | Role |
|------|------|
| **`inhibit-agent`** | **Standalone KEY agent (CLI).** USB-serial **CTS** + dest `host:port`. |
| **`inhibit-agent-gui`** | **Standalone KEY agent (GUI).** CTS in; dest `host:port` in the window. |
| **`wims-key-agent`** | **WIMS KEY agent.** Destinations from WIMS discovery. WIMS tree, not this repo. |
| **`inhibit-test`** | **Bench console.** Keyboard KEY stand-in. |
| `send_inhibit_hold.py` | Python stand-in (dev / scripted tests). |

**KEY agent** is the role. Standalone program: [docs/INHIBIT_AGENT.md](../docs/INHIBIT_AGENT.md).

Prefer **`inhibit-test`** (console) in docs and scripts.

## Build / install

```text
tools/inhibit-test/main.cpp       →  target inhibit-test       →  bin/inhibit-test[.exe]
```

## Behaviour

**KEY key = grave/backtick `` ` ``** (not Space — avoids false holds while typing).

1. **KEY assert** → **hold** immediately (`ttl_ms` = hold_timeout_ms, default 600) + keepalives ~200 ms.  
2. **KEYing monitor** classifies break-in CW vs continuous KEY; measures dit if break-in.  
3. **KEY open** → hang:  
   - **Break-in CW:** hang = **1.5 × word gap** (= 10.5 × dit), clamp ~315–1260 ms (≈40–10 WPM).  
   - **Continuous** (long mark / SSB / non-break-in): hang = **0** → **release hold** immediately.  
4. Hang done → **stop keepalives**, then **`ttl_ms: 0`** (no race).  

### Console (`inhibit-test`)

**Input focus (default):** `` ` `` and q/Esc only count when typed into **this** terminal. Use **`--global-keys`** for system-wide KEY.

**Two behaviours on one key, both always available:**

| Key | Behaviour |
|-----|-----------|
| `` ` `` (unshifted) | **Momentary.** KEY follows the key — assert while held, open on release. What a real KEY line does, and what break-in CW classification needs. |
| `~` (shift+grave) | **Latch on.** The hold stays asserted after you let go. |
| `` ` `` **or** `~` again | **Clears the latch, always.** `` ` `` then continues as momentary while held. |

The latch exists because momentary mode makes some tests impossible: the tool needs
keyboard focus to see the key held, but so does WSJT-X when you want to press Tune or
Enable Tx at the same time. Latch with `~`, slide over to WSJT-X, do what you need,
come back and press either key to release.

A latched KEY reads to the KEYing monitor as **one long continuous mark**, i.e. the
non-break-in / SSB class, so hang is 0 and release is immediate on the next press.
That is right by construction: the operator ends the transmission explicitly rather
than by pausing. Break-in CW hang is still exercised by the unshifted `` ` ``, which
stays momentary.

Note the latch survives loss of focus by design — the hold is held in the tool, not by
the keyboard. Releasing it does need the key press to be seen, so in the default mode
return focus to the terminal, or use `--global-keys` to press from anywhere.

**Linux requirement:** read access to `/dev/input` (group **`input`**). Without it the tool **refuses to start**.

```bash
sudo usermod -aG input "$USER"   # Linux; then full log out/in
inhibit-test --host 127.0.0.1 --port 22372 --station TEST-KEY --ttl-ms 600
inhibit-test --fixed-hang-ms 0
inhibit-test --global-keys             # ` and ~ readable from any window
```

**Tip:** hold `` ` `` ≥500 ms for hang=0 (continuous). Short taps use break-in hang unless fixed hang is 0.

Requires **Enable TX Inhibit** and RTS/DTR on the WSJT-X station under test.
