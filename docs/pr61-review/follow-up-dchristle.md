# Follow-up to dchristle on WSJTX/wsjtx#61

Status: **draft — do not post until reviewed**  
Related: [reply-to-dchristle.md](reply-to-dchristle.md) (first reply: leases + type 18 + dedicated port)  
Prep order: update/push `tx-inhibit` first, then paste the text below (present tense).

Copy everything below the horizontal rule into the PR conversation when ready.

---

Hi @dchristle — follow-up on the protocol points from your review.

This update lands **per-controller leases**, **type 18**, an **always-ephemeral UDP** inhibit listener, and a small **UI arming guard** on Enable TX Inhibit (PTT serial device) on the PR branch.

### Hold model

- Hold is no longer one global deadline.
- Each **Controller ID** owns one expiring lease row.
- PTT stays blocked while **any** lease is live (logical OR).
- A sender may only refresh or release **its own** row (`ttl_ms: 0` clears that controller only).
- Missed refresh still expires that row (deadman). The gate opens when no leases remain.

That matches the multi-multi case with two stations on one band (SSB and CW), either may be transmitting using their own inhibit mechanism. This enables WSJT-X to participate as well.

### Wire encoding

- Inbound hold / keepalive / release on the dedicated inhibit listener is now `NetworkMessage::TxInhibit` **type 18**, with your field list:

```
Id (target)      utf8   (empty = any instance at this address; non-empty must match this instance's Id or the datagram is dropped)
Controller ID    utf8   (required, non-empty)
TTL ms           quint32
Station          utf8   (badge text)
```

- The Id rule above is the published semantic and is now enforced in the gate:
  empty Id = any instance at this port; non-empty must match this instance’s Id.
- Legacy JSON is no longer accepted. There were no users to accommodate.
- `inhibit-agent`, `inhibit-test`, and the send helpers now emit type 18 and require a Controller ID.

### Receive path: always-ephemeral UDP inhibit listener

Terminology in this note: **UDP inhibit endpoint** = `host:<udp-port>` where the gate listens for type-18 holds. **PTT serial device** = Settings → Radio → Port for RTS/DTR (e.g. `/dev/ttyUSB…` or `COMx`). Those are different things.

After more integration testing of the overall WIMS system I decided to make the UDP inhibit listener **always ephemeral** — no fixed well-known UDP port. Reasons:

- One way of handling the UDP inhibit endpoint.
- Two WSJT-X instances on one host no longer need a busy-UDP-port fallback. Sharing one UDP port with `ShareAddress` did not give reliable per-instance delivery.
- Each instance gets its own UDP port and is addressed unambiguously, the same way each instance already has its own command endpoint.
- Discovery is already solved on our side. Type 17 (`InhibitStatus`) carries the bound **UDP** inhibit port on every bind and every 15 s pulse, so WIMS builds the per-band KEY-agent target list (`host:<udp-port>`) from the same UDP Server stream it already reads for decodes. For a standalone seat without WIMS, the discovery-capable `inhibit-agent` variant you suggested would read the same type 17 to find that UDP endpoint. That covers every station configuration we have identified so far.

What I kept is a **separate socket for the gate**, serviced on the transceiver thread next to the PTT line, rather than folding type 18 into the MessageClient dispatch on the GUI thread. That is the latency and isolation point from my earlier comment. The wire format is identical either way, so a controller that speaks type 18 to the UDP endpoint announced in type 17 does not care which socket is behind it.

So this update includes:

1. Type 18 with per-controller leases, as above.
2. Inhibit listener binds an ephemeral **UDP** port; type 17 announces it on bind, on clear, and every 15 s while TX Inhibit is enabled.
3. Controllers learn that **UDP** endpoint from type 17 on the UDP Server stream, exactly as they learn the command endpoint from Heartbeats today.
4. No fixed UDP inhibit port and no busy-UDP-port warning path.

### UI: Enable TX Inhibit vs PTT serial device

**Enable TX Inhibit** is available only when PTT method is RTS or DTR **and** the **PTT serial device** (Settings → Radio → Port) is non-empty. Clearing that serial device clears the checkbox. What the gate *does* when enabled is unchanged; this only tightens when the control can be armed so Inhibit cannot look “on” with no RTS/DTR device selected.  This solved an issue where WSJT-X would report "Inhibit" if enabled even if not RTS or DTR device was set.

Related Settings work on the **PTT/CAT serial device list** (custom paths / udev symlinks such as `/dev/ttyUSB9700a`, keep the line edit writable, refresh the list when Settings opens) is **not** in this PR. I will publish that as a **separate PR on its own timeline**, so it can be reviewed as Configuration UX without blocking or mixing with the inhibit protocol.

### InhibitStatus (type 17): when it is sent

Type 17 was already in this PR as outbound telemetry on the UDP Server path. The message layout is unchanged.

**When** it is sent is what changed. Previously it went out at startup and on hold or badge changes, which was not reliable for building a target list: a controller that started after WSJT-X, or an enable toggle that produced no usable announce, left the list empty. It is now also sent when the **UDP** inhibit listener binds or clears, and every `NetworkMessage::pulse` seconds (15 s) while TX Inhibit stays enabled, the same cadence as Heartbeat.

With an always-ephemeral UDP inhibit listener, type 17 is the only place that **UDP** port appears on the bus, so this cadence becomes part of the protocol rather than a convenience. It also carries the live gate picture (held or not, by whom, counters) for triage.

### Still true either way

- Opt-in, off by default  
- Lease TTL deadman  
- Inhibit ≠ Halt (sequencing and audio continue under a hold)  
- RTS/DTR-only enable  
- Never leave RTS asserted on serial open  

Happy to take a development build from you against this dialect. After September VHF Contest I will have more time.

---

## Notes for continued review (not for posting)

### Attribution

| Type | Who | Role |
|------|-----|------|
| **17 `InhibitStatus` (Out)** | **wa1hco** — already in the original PR | dchristle noted the PR already used outbound InhibitStatus; listed it as Out 17 next to his sketch |
| **18 `TxInhibit` (In)** | **dchristle** | Proposed inbound command, field list (Controller ID, TTL, Station), and lease model |

### Original purpose of type 17 (“telemetry”)

First gate commit (`2ac1be573`, 2026-08-03) called it “plane-A InhibitStatus telemetry” and pointed at WIMS §11.4. Fields: inhibit port, gate state, source station, counters.

WIMS design §11.4 treated one datagram as two jobs:

1. **Port / capability announce** — so WIMS can learn `instance → inhibit listen port` (KEY-agent target list; ephemeral bind).
2. **Live gate picture** — held or not, who is holding, counters for plumbing triage.

In-tree note at the time: “Plane A: InhibitStatus announces bind port + state.”

So “telemetry” was the in-repo label from day one. The recent change is mainly **cadence** (bind/clear + 15 s pulse), after startup/enable-only proved unreliable for list building — not a new consumer story invented for the dchristle thread.

### Related local files

| File | Role |
|------|------|
| [reply-to-dchristle.md](reply-to-dchristle.md) | First draft reply (leases, type 18, dedicated port push-back) |
| [WIMS-wsjtx-PR61-protocol-review.md](WIMS-wsjtx-PR61-protocol-review.md) | Working notes from the review |
| [docs-audit.md](docs-audit.md) | Docs consistency audit vs type-18 authority |
| [grok-review-tx-inhibit-proposed-2026-09-07.md](grok-review-tx-inhibit-proposed-2026-09-07.md) | Code review of proposed PR update (`34a1b05`→`main`) |
| [follow-up-dchristle-ste-lite.md](follow-up-dchristle-ste-lite.md) | Same follow-up in lite Simplified Technical English |
| `scratch/dchristle-pr61-review.txt` | Full text of his 2026-08-25 PR review comment |
