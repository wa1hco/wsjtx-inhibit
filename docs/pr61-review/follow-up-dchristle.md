# Follow-up to dchristle on WSJTX/wsjtx#61

Status: **draft — do not post until reviewed**  
Related: [reply-to-dchristle.md](reply-to-dchristle.md) (first reply: leases + type 18 + dedicated port)  
Code on `wa1hco/wsjtx-inhibit` `main`: `57dd732` (not yet on PR head `tx-inhibit`)

Copy everything below the horizontal rule into the PR conversation when ready.

---

Hi @dchristle — follow-up on the protocol points from your review.

I have made two of the changes, **per-controller leases** and **type 18**, in `wa1hco/wsjtx-inhibit` `main` (commit `57dd732`). This is not yet on the `tx-inhibit` branch that feeds this PR; I will push that head next if you want it here for packet testing.

### Hold model

- Hold is no longer one global deadline.
- Each **Controller ID** owns one expiring lease row.
- PTT stays blocked while **any** lease is live (logical OR).
- A sender may only refresh or release **its own** row (`ttl_ms: 0` clears that controller only).
- Missed refresh still expires that row (deadman). The gate opens when no leases remain.

That matches the multi-multi case with two stations on one band (SSB and CW), either may be transmitting using their own inhibit mechanism.  This enables WSJT-X to participate as well.

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

### Receive path: proposing to drop the fixed port

I have been thinking about your fixed-port question and I am now leaning toward making the inhibit listener **always ephemeral**, no 22372 at all. Reasons:

- One way of handling the inhibit port, seems more tasteful.
- Two WSJT-X instances on one host no longer need a busy-port fallback. Each gets its own port and is addressed unambiguously, the same way each instance already has its own command endpoint.
- Discovery is already solved on our side. Type 17 (`InhibitStatus`) carries the bound inhibit port on every bind and every 15 s pulse, so WIMS builds the per-band KEY-agent target list from the same UDP Server stream it already reads for decodes. For a standalone seat without WIMS, the discovery-capable `inhibit-agent` variant you suggested would read the same type 17 to find its target. That covers every station configuration we have identified so far.

What I would keep is a **separate socket for the gate**, serviced on the transceiver thread next to the PTT line, rather than folding type 18 into the MessageClient dispatch on the GUI thread. That is the latency and isolation point from my earlier comment; The wire format is identical either way, so a controller that speaks type 18 to the port announced in type 17 does not care which socket is behind it.

So the concrete proposal is:

1. Type 18 with per-controller leases, as above.
2. Inhibit listener binds an ephemeral port; type 17 announces it on bind, on clear, and every 15 s while TX Inhibit is enabled.
3. Controllers learn the port from type 17 on the UDP Server stream, exactly as they learn the command endpoint from Heartbeats today.
4. No fixed port and no busy-port warning path.

**Local implementation status (wsjtx-inhibit `main`, not yet on PR branch `tx-inhibit`):** items 1–4 and Id matching are committed; `inhibit-agent-gui` shows **NEED GATE** until Apply; INSTALL* matches ephemeral ports. Still open: type-17 auto-discovery inside standalone `inhibit-agent` (operator or WIMS still supplies `host:port`). Settings Port-list UX (preserve typed paths / refresh on open) is on branch `settings-serial-port-ux` for a separate PR.

Does that address your concern about the second endpoint? If so I will push the result onto this PR branch for packet testing.

### InhibitStatus (type 17): when it is sent

Type 17 was already in this PR as outbound telemetry on the UDP Server path. I am not changing the message layout.

I am changing **when** it is sent. Previously it went out at startup and on hold or badge changes, which was not reliable for building a target list: a controller that started after WSJT-X, or an enable toggle that produced no usable announce, left the list empty. It is now also sent when the inhibit port binds or clears, and every `NetworkMessage::pulse` seconds (15 s) while TX Inhibit stays enabled, the same cadence as Heartbeat.

With an always-ephemeral inhibit port, type 17 is the only place the port appears on the bus, so this cadence becomes part of the protocol rather than a convenience. It also carries the live gate picture (held or not, by whom, counters) for triage.

### Still true either way

- Opt-in, off by default  
- Lease TTL deadman  
- Inhibit ≠ Halt (sequencing and audio continue under a hold)  
- RTS/DTR-only enable  
- Never leave RTS asserted on serial open  

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
| `scratch/dchristle-pr61-review.txt` | Full text of his 2026-08-25 PR review comment |
