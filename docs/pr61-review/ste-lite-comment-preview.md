# STE-lite preview: comments in PR inhibit code

Scope: comments in the inhibit-related PR code (gate, NetworkMessage, MainWindow
announce path, Configuration Inhibit guards, agents). Not a full rewrite of
stock WSJT-X comments.

Style: lite Simplified Technical English.

---

## Already close (little or no change)

These already read as short WHY comments:

| Location | Current |
|----------|---------|
| `HamlibTransceiver.cpp` | `// Dummy PTT port: start the gate, do not open a UART.` |
| `HamlibTransceiver.cpp` | `// Same Id MessageClient puts on Heartbeat / InhibitStatus (type 17).` |
| `TxInhibitSimPort.hpp` | Dummy PTT port name… Starts the gate without opening a UART. |
| `agent.hpp` | `// 0 = unset; must be set from InhibitStatus / operator.` |
| `mainwindow.cpp` | `// Announce on bind/clear so controllers learn the port without a hold.` |

---

## Would change

### 1. `TxInhibitGate.cpp` — bind comment

**Before**

```cpp
  // Bind an ephemeral port (OS-assigned). Controllers learn the port from
  // InhibitStatus (type 17) on the UDP Server stream. Each instance on a
  // host gets its own port.
```

**After**

```cpp
  // Bind an ephemeral UDP inhibit port (OS-assigned).
  // Type 17 reports that UDP port on the UDP Server stream.
  // Each WSJT-X instance gets its own UDP port.
```

Why: “port” alone is ambiguous. One idea per sentence.

---

### 2. `TxInhibitGate.hpp` — `set_instance_id`

**Before**

```cpp
  // NetworkMessage Id for this station (usually QApplication::applicationName()).
  // Type-18 datagrams with a non-empty target Id must match; empty Id = any.
```

**After**

```cpp
  // NetworkMessage Id for this station (usually QApplication::applicationName()).
  // Type-18 empty target Id = any instance at this UDP endpoint.
  // Non-empty target Id must match this station Id.
```

---

### 3. `TxInhibitLogic.hpp` — file header (trim)

**Before** (excerpt)

```cpp
// Pure, I/O-free TX Inhibit logic (parse + per-controller leases)
// ...
//   Hold = logical OR of per-controller leases. Each NetworkMessage::TxInhibit
//   (type 18) refreshes or releases only that Controller ID's row. Missed
//   refresh → that row expires (deadman). Agent hang is KEY-agent only.
```

**After**

```cpp
// Pure TX Inhibit logic: parse type 18 and manage per-controller leases.
// No I/O.
//
// assert PTT  ⇔  want_tx  and  not hold
// hold        ⇔  any live per-controller lease
//
// Type 18 refreshes or releases only that Controller ID.
// Missed refresh expires that row (deadman).
// Agent hang lives in the KEY agent only. See docs/TX_INHIBIT.md.
```

---

### 4. `TxInhibitLogic.hpp` — `target_id` field

**Before**

```cpp
  QString target_id;      // NetworkMessage Id; empty = any instance at this port
```

**After**

```cpp
  QString target_id;      // NetworkMessage Id; empty = any instance at this UDP endpoint
```

---

### 5. `TxInhibitLogic.hpp` — `set_instance_id` / `on_datagram`

**Before**

```cpp
  // NetworkMessage Id of this WSJT-X instance. Non-empty target Id on a
  // type-18 datagram must match; empty target Id is accepted (any instance
  // at this UDP address/port).
  ...
  // Return value:
  //   true  = the hold *level* flipped (free↔held)
  //   false = level unchanged — keepalives, invalid packets, Id mismatch,
  //           redundant releases, or another controller still holding.
```

**After**

```cpp
  // NetworkMessage Id of this WSJT-X instance.
  // Empty type-18 target Id = accept.
  // Non-empty target Id must match this instance Id.
  ...
  // Returns true if the hold level changed (free↔held).
  // Returns false for keepalives, invalid packets, Id mismatch,
  // redundant releases, or another live lease.
```

---

### 6. `Network/NetworkMessage.hpp` — InhibitStatus / TxInhibit protocol comments

**Before** (InhibitStatus cadence block — long)

```text
Optional telemetry when TX Inhibit is enabled ...
Emitted on hold-level or badge-text changes, when the inhibit
listen port binds or clears, and periodically every
NetworkMessage::pulse seconds ... so late joiners learn the inhibit
port. Counter-only bumps wait for the next pulse or level/badge
change. Travels on the configured UDP Server path ...
Inhibit port: OS-assigned ephemeral UDP listen port for KEY-agent
holds. ...
```

**After**

```text
Outbound when TX Inhibit is enabled (Settings → Radio).
Send on: hold-level change, badge-text change, UDP inhibit bind/clear,
and every NetworkMessage::pulse seconds while enabled.
Counter-only bumps wait for the next pulse or level/badge change.
Path: configured UDP Server (unicast or multicast).

UDP inhibit listen port: OS-assigned ephemeral port for type-18 holds.
Inhibited: any per-controller lease is live.
Source station: badge text.
Counters: hold_rx, release_rx, expiries, invalid.
```

**Before** (TxInhibit body prose)

```text
KEY-agent / controller hold command. Nonzero TTL creates or
refreshes that controller's lease; zero TTL releases only that
controller's lease. Hold is the logical OR of live leases.
Controller ID must be non-empty. Station is human badge text.
Empty Id matches any instance at this UDP address/port; a
non-empty Id must equal this instance's NetworkMessage Id or
the datagram is ignored. Controllers learn the listen port from
InhibitStatus (type 17). See docs/TX_INHIBIT.md.
```

**After**

```text
Inbound KEY-agent hold command.
Nonzero TTL creates or refreshes that Controller ID lease.
Zero TTL releases that Controller ID lease only.
Hold = logical OR of live leases.
Controller ID must be non-empty.
Station is badge text.
Empty Id = any instance at this UDP endpoint.
Non-empty Id must match this instance NetworkMessage Id, else ignore.
Controllers learn the UDP inhibit port from InhibitStatus (type 17).
See docs/TX_INHIBIT.md.
```

---

### 7. `Configuration.cpp` — Inhibit enablement

**Before**

```cpp
  // Typing a custom device path must update Enable TX Inhibit enablement.
  ...
  // TX Inhibit needs RTS/DTR and a real Port string (typed path or list item).
```

**After**

```cpp
  // Re-check Enable TX Inhibit when the PTT serial device text changes.
  ...
  // Enable TX Inhibit only for RTS/DTR with a non-empty PTT serial device.
```

---

### 8. `mainwindow.cpp` — tooltip / announce

**Before**

```cpp
  // Always read from Configuration; a cached copy can disagree after bind/clear.
  ...
  // Keep capability/port announcements alive while the feature is enabled so
  // WIMS / KEY-list builders that join after startup still see type 17.
```

**After**

```cpp
  // Read the UDP inhibit port from Configuration each time.
  ...
  // Keep type-17 announces running while TX Inhibit is enabled.
```

Operator-visible tooltip strings (tr(...)) are UI copy, not code comments.
Those can get a separate STE-lite pass if you want.

---

### 9. `tools/inhibit-agent/main_gui.cpp` — file header

**Before**

```cpp
// inhibit-agent-gui — CTS KEY in, dest host:port out.
// Gate address is editable (host:port from InhibitStatus type 17). Serial KEY is
// auto-picked (Keyline / only USB-serial) unless --port is given.
```

**After**

```cpp
// inhibit-agent-gui — CTS KEY in; UDP inhibit endpoint out.
// Gate field: UDP host:<udp-port> from InhibitStatus (type 17).
// KEY serial device: auto-pick unless --port is given.
```

```cpp
  // Do not arm KEY→hold until Gate host:port is set (ephemeral port from type 17).
```

**After**

```cpp
  // Do not arm KEY→hold until the UDP inhibit endpoint is set.
```

---

### 10. `tools/inhibit-agent/main_cli.cpp` / `send_inhibit_hold.py` headers

**Before**

```text
// Dest port comes from WSJT-X InhibitStatus (type 17) / status-bar tooltip.
```

**After**

```text
// Dest UDP port comes from InhibitStatus (type 17) or the status-bar tooltip.
```

---

### 11. `inhibit-test/main.cpp` — banner (optional trim)

The long file banner is useful domain knowledge. STE-lite would split long lines
and say **UDP** where needed. Full rewrite is optional; highest value is the
`--port` note:

**Before:** `// --port is required (from InhibitStatus / tooltip).`  
**After:** `// --port is required: UDP inhibit port from InhibitStatus / tooltip.`

---

## Summary

| Kind | Count |
|------|-------|
| Already fine | ~6 short comments |
| Clear STE-lite wins (ambiguity / length) | ~11 sites above |
| Leave alone unless you ask | Long `inhibit-test` banner; operator `tr()` strings |

Largest gain: **NetworkMessage.hpp** protocol comments and any bare **“port”** that could mean UDP or serial.

Say if you want these edits applied to the tree.
