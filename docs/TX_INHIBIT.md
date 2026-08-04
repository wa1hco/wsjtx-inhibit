# TX Inhibit and KEY agent

This document is the **design authority for this repository**: how
**wsjtx-inhibit** gates PTT, and how a **KEY agent** drives that gate.

**Operators (install / Spacebar test):** [INSTALL.md](../INSTALL.md)  
**Code:** `TxInhibit/`, `Configuration.cpp` (PTT handoff), status-bar badge in
`widgets/mainwindow.cpp`

---

## 1. Roles

| Role | Where it runs | Job |
|------|----------------|-----|
| **wsjtx-inhibit (gate)** | On the WSJT-X seat PC | Owns RTS/DTR PTT to *this* radio. Holds the line low when inhibited. |
| **KEY agent** | On the PC (or process) that can see the **priority** radio’s KEY/PTT sense | Watches that KEY line; while the priority station is transmitting (plus hang), sends **hold** UDP datagrams to every gate that must stay quiet. |
| **Bench helper** | Same PC as the seat (or LAN) | `bin/inhibit-spacebar` (next to `wsjtx`) or `tools/send_inhibit_hold.py`. Same datagrams; no hardware KEY. |

The gate does not need to know which product implements the KEY agent. Any
program that speaks the UDP protocol below is a valid agent.

```text
  Priority radio (SSB/CW, …)
           │ KEY / amp-key / PTT sense
           ▼
      ┌───────────┐     hold / keepalive / release (UDP)
      │ KEY agent │ ─────────────────────────────────────┐
      └───────────┘                                      │
                                                         ▼
                                              ┌────────────────────┐
  WSJT-X intent ──► │  wsjtx-inhibit gate │ ── RTS/DTR ──► radio PTT
                    │  radiate = intent    │
                    │         ∧ ¬inhibit   │
                    └────────────────────┘
```

Inhibit source is **UDP only** (KEY agent or local bench helper). Local CTS KEY
on the PTT dongle is **out of this build** (§5).

---

## 2. Gate behaviour (this program)

- **WSJT-X sequencing and audio stay the same.** Inhibit only forces the
  **physical PTT key line** low.
- Line equation: **`RTS or DTR = intent ∧ ¬inhibit`**.
- Inhibit is true when an unexpired **UDP hold** is active (KEY agent or test tool).
- Default UDP listen port: **22372** (IPv4). If that port is busy, the gate binds
  an **ephemeral** port and reports it (status path / `InhibitStatus` for tools
  that care). Prefer leaving 22372 free on the seat.
- Status bar (red): `TX INHIBITED` or `TX INHIBITED — held by <station>`.

### WSJT-X PTT source setup (summary)

1. **PTT method** = **RTS** or **DTR**.
2. **PTT port** = a real serial device (`COMx` / `/dev/ttyUSBx`).  
   That may be the **same** COM as CAT (shared USB CAT + RTS/DTR — a normal
   radio setup) or a separate PTT adapter.  
   The list entry **`CAT`** means OmniRig-style proxy; the gate needs a real
   device name it can open (same name as CAT is fine).
3. Wire RTS or DTR → radio PTT/SEND (or use the radio’s USB SEND / PC KEYING map).
   Leave radio **VOX** off for clean tests.
4. Point the KEY agent at this seat’s host and inhibit port (usually `22372`).

### Shared USB CAT + RTS/DTR (what operators actually do)

Many modern contest/digital radios expose **one USB cable** that provides:

- a **virtual COM port** for CAT, and  
- the same port’s **RTS** and/or **DTR** modem lines, which the radio can map to **PTT / CW / FSK**.

**Shared COM is a valid configuration.** Operators routinely run CAT and RTS/DTR
PTT on that one port (especially Icom, Elecraft, and many Yaesu USB rigs). N1MM
and similar loggers document that **CAT control and DTR/RTS PTT on one port are
compatible** when the radio is **not** using RTS for hardware handshaking.

They also commonly use **CAT PTT only** (no RTS/DTR) for casual FT8, or a
**second path** (WinKeyer, ACC jack, DigiRig, dedicated USB-serial) when several
programs must share the station cleanly.

For **wsjtx-inhibit**, set **PTT method** = RTS or DTR and **PTT port** = that
**same real COM name** as CAT (not the special list entry “CAT”). The gate
drives the modem line; CAT still uses the rig’s serial path with
**Handshake = None**.

#### Which brands map USB RTS/DTR to radio PTT?

| Brand (typical contest HF) | RTS/DTR on radio USB can key TX? | Notes |
|----------------------------|----------------------------------|--------|
| **Icom** (7300, 7610, 705, 9700, …) | **Yes** | Menu **USB SEND / Keying**: assign **USB SEND** = RTS or DTR (PTT); optional CW/RTTY keying on the other line. |
| **Elecraft** (K3S, K4, …) | **Yes** | Menu assigns USB (or USB-PC1/PC2) **RTS/DTR** → **PTT / KEY / FSK**. |
| **Yaesu** (FTDX10, FTDX101, FT-710, FT-991A, …) | **Yes (usual)** | **PC KEYING** = RTS or DTR; set **CAT RTS** (flow control) **OFF** so RTS is not used as handshake. |
| **Kenwood** (TS-590SG, TS-890S, …) | **Usually no on built-in USB** | USB is mainly **CAT commands**; hardware PTT is typically **ACC/mic**. External USB-serial → ACC is the usual hardware path. |
| **FlexRadio** (6400/6600/…) | **Not radio USB** | SmartSDR may watch a **PC virtual COM** RTS/DTR and then software-key the radio. Physical PTT is RCA/ACC, not classic “USB SEND”. |

#### Common setup problems (reports from the field)

| Symptom / pitfall | What is going wrong | What to try |
|-------------------|---------------------|-------------|
| Flaky CAT, stuck TX, or PTT never works | **RTS used as flow control** (handshake) instead of keying | App: **Handshake = None**. Radio: disable CAT RTS handshake; enable **USB SEND** / **PC KEYING** for PTT. |
| Radio keys when software opens the port | DTR/RTS **polarity** or “always on” | Check OmniRig / logger RTS/DTR forced state; radio USB SEND assignment; try the other line (RTS vs DTR). |
| “Worked yesterday, PTT dead today” | **Another program** changed COM line settings (FLDIGI, second logger, flrig) | One owner of the COM, or a proper port broker; restart seat software after other apps close the port. |
| Test CAT green, Test PTT “works” but no RF / no audio | Often **not** an RTS bug | DATA mode, USB sound device, levels; confirm TX light vs actual RF. |
| Wrong digi mode or CW key instead of PTT | Icom **USB Keying (CW/RTTY)** vs **USB SEND** mixed up | PTT → **USB SEND** = RTS or DTR; leave CW/RTTY keying off unless you need them. |
| Port open blips TX | OS/driver toggles DTR/RTS on open | Known with some USB-serial chips and Linux defaults; dedicated PTT adapter + correct polarity helps. |
| Multi-app contest station fails | N1MM vs digi program **who owns PTT** | Follow logger “Digi” / port-handoff rules; don’t double-key the same lines from two apps. |
| “Inhibit never holds” | Seat uses **PTT method = CAT** only | Gate only gates RTS/DTR. Switch to **RTS/DTR** on a real serial port the gate opens. |

#### Configuration options for **wsjtx-inhibit** seats

| Option | Notes |
|--------|--------|
| **Same USB COM for CAT + RTS/DTR** | **Supported / normal** on Icom, Elecraft, Yaesu when menus match the checklist below. |
| **Separate USB-serial for PTT only** | Also fine — DigiRig keyline, second dongle → ACC/SEND, multi-app split. |
| **CAT PTT only** | Will not be inhibit-gated. Switch to RTS/DTR on a real serial port. |
| **Kenwood / Flex** | Plan on **external** keyline or Flex’s virtual-PTT model. |

**Operator checklist for one USB cable (shared CAT + RTS/DTR):**

1. Handshake **None** in the app (RTS must not be CAT flow control).  
2. Radio menu: this line = **USB SEND** / **PC KEYING** / PTT, not flow control.  
3. WSJT-X **PTT method** = **RTS** or **DTR** (not CAT method).  
4. **PTT port** = the same real `COMx` / tty as CAT (not the special list value “CAT”).  
5. Only **one** program drives that COM’s modem lines at a time.  
6. Confirm with **Test PTT** *and* RF/ALC (or TX light + power meter), not only “CAT green.”

---

## 3. KEY agent (generic design)

A **KEY agent** is a small program whose only job is:

1. **Sense** whether the priority station’s KEY is closed (transmitting or about to).
2. **Hold the band** for every WSJT-X seat that must stay quiet: send UDP
   **hold** datagrams, and **refresh** them often enough that the gate’s
   **deadman** does not fire while the priority station is still keyed (§3.1).
3. **Release** the band after KEY has been open long enough (**hang**), by
   sending `ttl_ms: 0` (or simply stopping keepalives and letting deadman expire
   — explicit release is preferred for clean UX).

### 3.1 TTL, keepalive, and deadman

These three terms describe how a hold stays armed without a permanent connection:

| Term | Who sets it | Meaning |
|------|-------------|---------|
| **TTL** (`ttl_ms` in the JSON) | KEY agent, in each hold packet | **Time-to-live:** how long this hold remains valid from the moment the **gate receives** the packet. Example: `ttl_ms: 600` means “treat the band as held for 600 ms from now.” `ttl_ms: 0` means **release**. Allowed non-zero range: 100…30000 ms. |
| **Keepalive** | KEY agent, while KEY is still closed (and during hang) | A **repeat hold** packet with a fresh `ttl_ms`, sent on a short period (typically every ≤ 200 ms). Each keepalive **re-arms** the gate’s deadline. |
| **Deadman** | Gate, automatically | If no new hold/keepalive arrives before the current deadline, the gate **clears the hold by itself** so a crashed agent cannot leave the seat inhibited forever. |

**How they fit together**

```text
  Agent KEY down  →  send hold (ttl_ms = 600)
  Agent (while held) → send keepalive every ~200 ms (each with ttl_ms = 600)
  Gate               → deadline = time_of_last_hold + ttl_ms
  If agent goes silent → deadline passes → deadman clears inhibit
  Agent KEY up (+ hang) → send release (ttl_ms = 0)  [preferred]
```

The gate only stores the deadline from `ttl_ms` and expires it (deadman). Hang
is agent policy (§3.3).

### 3.2 Inputs the agent may use

| Input | Notes |
|-------|--------|
| USB-serial **CTS** (or other modem pin) | Common: KEY contact → opto/interface → CTS. Event-driven wait where the driver supports it; else short poll. |
| Parallel / GPIO / other sense | Implementation detail of the agent. |
| Manual / test UI | Spacebar **level** — `inhibit-spacebar` or `tools/send_inhibit_hold.py --interactive`. |

The agent owns **debounce and hang** for its KEY input. The gate only applies
TTL / deadman to UDP messages.

### 3.3 Why hang lives in the agent

CW and semi-break-in KEY lines open between elements and words. If the agent
released on every open edge, WSJT-X seats would unlock between dits — bad for
the one-signal-per-band rule and hard on PTT relays.

**Hang:** after KEY opens, the agent **keeps sending hold keepalives** until the
line has been continuously open for the hang time, then sends **release**.

Sizing is an agent policy. A useful experiment (see **§6** `inhibit-spacebar`):

- Short KEY closures (CW elements) → estimate dit, hang ≈ **8×dit** (clamped).  
- Long KEY closures (SSB-style) → short debounce hang only.

While the agent wants the band held, datagrams must arrive often enough that
`ttl_ms` does not expire (deadman must not fire during the transmission).

### 3.4 Targets

The agent is configured with one or more **gate endpoints**:

```text
host:port   e.g.  192.168.1.40:22372
```

Unicast UDP is enough for a small multi-op. Each gate parses independently.
Multiple agents can send to the same gate; the gate keeps a **single** hold
deadline (last valid hold wins; any valid release clears).

Wire format and field rules: **§4**. Bench stand-ins: `bin/inhibit-spacebar`
or `tools/send_inhibit_hold.py`.

---

## 4. UDP protocol (gate ↔ agent)

**Transport:** UDP, payload = UTF-8 JSON object, max **512** bytes.

**Well-known port:** **22372** (gate listens; agent sends).

### Fields

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `tx_inhibit` | int | yes | Protocol id; must be **1** |
| `ttl_ms` | int | yes | Hold lifetime in ms, or **0** = release. If non-zero: 100…30000 |
| `station` | string | recommended | Shown on badge (`held by …`) |
| `band` | string | optional | Informational (gate does not filter by band today) |
| `seq` | number | optional | Agent sequence; informational |

### Hold (and keepalive — same shape)

```json
{"tx_inhibit":1,"ttl_ms":600,"station":"ROY-222-SSB","band":"222","seq":1}
```

Each hold **re-arms** the deadline to `now + ttl_ms` (last packet wins).

### Release

```json
{"tx_inhibit":1,"ttl_ms":0,"station":"ROY-222-SSB","band":"222","seq":2}
```

### Deadman

If no new hold arrives before the deadline, the gate clears inhibit by itself.
Agents must keepalive for long transmissions.

### Invalid packets

Malformed JSON, wrong `tx_inhibit`, bad `ttl_ms`, or oversized payload: **ignored**
(no state change). Counters exist in logic for diagnostics.

---

## 5. Local CTS KEY — not in this build

**Decision:** keep **CTS out of wsjtx-inhibit** for now. The seat binary stays
simple: **UDP hold only** + RTS/DTR drive.

**Why:** On real COM ports, CTS is often floating or driven by something the
operator cannot control. Field testing showed **intermittent, uncontrollable
inhibits** when the gate polled CTS. That is worse than requiring a KEY agent
(or Spacebar helper) on UDP.

**Two-radio idea** (SSB KEY → CTS, digi PTT ← RTS on the same USB-serial) is
attractive on paper, but until CTS is **opt-in** and safe:

- Use a **KEY agent** (own sense path or second dongle) → UDP.  
- Or run **`inhibit-spacebar`** / agent on the same PC (localhost UDP).

**If CTS returns later:** require an explicit **Settings** enable (default off),
document polarity/idle, and never sample CTS unless the operator turns it on.

---

## 6. Testing inhibit locally

You can exercise the gate on one PC **without** a real KEY agent or multi-op
setup. Run wsjtx-inhibit with RTS/DTR on a real serial port, then send the same
UDP hold packets a KEY agent would send (default target **127.0.0.1:22372**).

Expect a red **TX INHIBITED** badge; radio PTT stays low during hold.

### `inhibit-spacebar` (ships next to `wsjtx`)

Built and installed with the app as:

| Platform | Path in the install / portable tree |
|----------|-------------------------------------|
| **Windows** | `bin\inhibit-spacebar.exe` (same folder as `bin\wsjtx.exe`) |
| **Linux** | `bin/inhibit-spacebar` (or on `PATH` as `inhibit-spacebar`) |

KEY-agent stand-in only: sends hold/keepalive/release UDP.

| Action | Effect |
|--------|--------|
| **Space held down** | KEY **down** — hold + keepalives |
| **Space released** | KEY **up** — **hang** (adaptive or fixed), then release (`ttl_ms: 0`) |
| **q** or **Esc** | Release immediately and quit |

**CW on Spacebar:** short presses are treated as CW elements. Hang is **adaptive**
by default: estimate a dit from short closures, hang ≈ **8×dit** (clamped
200–1000 ms). Long presses (≥ 500 ms, SSB-style) use a short debounce hang only.

Typical use:

1. Start **wsjtx-inhibit** with PTT = RTS or DTR on a real serial port.  
2. Run **`bin\inhibit-spacebar.exe`** (or `inhibit-spacebar` on Linux) from the
   same install tree.  
3. **Hold Space** — badge **TX INHIBITED**; radio stays unkeyed.  
4. **Release Space** — after hang, badge clears; normal PTT works.  
5. Key dits/dahs on Space to see hang/dit estimates printed on the console.

Defaults: host `127.0.0.1`, port **22372**, station `TEST-SSB`. Options:

```text
inhibit-spacebar --host 127.0.0.1 --port 22372 --station TEST-SSB --ttl-ms 600
inhibit-spacebar --fixed-hang-ms 500
```

If the gate bound an ephemeral port instead of 22372, pass that `--port` or free
22372 and restart wsjtx-inhibit.

**Linux note:** Space level uses `/dev/input` (may need group `input`).  
**Windows:** Space level via `GetAsyncKeyState`.

### Python: `tools/send_inhibit_hold.py`

Same protocol and Space **level** behaviour. From a source checkout:

```bash
python3 tools/send_inhibit_hold.py --interactive

python3 tools/send_inhibit_hold.py --ttl-ms 3000 --station TEST
python3 tools/send_inhibit_hold.py --ttl-ms 0
```

Operator-oriented install checklist: [INSTALL.md §6](../INSTALL.md#6-test-inhibit-with-the-spacebar-helper).

---

## 7. Code map

| Piece | Location |
|-------|----------|
| Parse + deadline + badge text | `TxInhibit/TxInhibitLogic.hpp` |
| UDP + serial RTS/DTR + thread | `TxInhibit/TxInhibitGate.{hpp,cpp}` |
| Steal RTS/DTR from hamlib; `set_intent` | `Configuration.cpp` `open_rig` / `transceiver_ptt` |
| Status-bar badge | `widgets/mainwindow.cpp` |
| Optional UDP `InhibitStatus` telemetry | `Network/NetworkMessage.hpp`, `MessageClient` |
| KEY-agent stand-in | `bin/inhibit-spacebar`; `tools/send_inhibit_hold.py` |
