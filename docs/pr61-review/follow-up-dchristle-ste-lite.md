# Follow-up to dchristle on WSJTX/wsjtx#61 (STE-lite draft)

Status: **draft — do not post until reviewed**  
Related: [follow-up-dchristle.md](follow-up-dchristle.md) (full draft)  
Style: lite Simplified Technical English (short sentences, one idea each, active voice, fixed terms). Not full ASD-STE100.

Prep order: update/push `tx-inhibit` first, then paste the text below (present tense).

Copy everything below the horizontal rule into the PR conversation when ready.

---

Hi @dchristle — this is a follow-up on your protocol review.

This PR update adds four things: per-controller leases, type 18 holds, an always-ephemeral UDP inhibit listener, and a small UI guard on Enable TX Inhibit.

### Hold model

- Hold is not one global deadline.
- Each Controller ID owns one lease row with an expiry time.
- PTT stays blocked while any lease is live (logical OR).
- A sender may refresh or release only its own row (`ttl_ms: 0` clears that Controller ID only).
- If a refresh is missing, that row expires (deadman).
- The gate opens when no leases remain.

This supports two KEY stations on one band (SSB and CW). Each station can hold digi seats. WSJT-X can take part in that model.

### Wire encoding

Inbound hold, keepalive, and release use `NetworkMessage::TxInhibit` type **18**:

```
Id (target)      utf8   (empty = any instance at this UDP endpoint; non-empty must match this instance Id or the datagram is dropped)
Controller ID    utf8   (required, non-empty)
TTL ms           quint32
Station          utf8   (badge text)
```

The gate enforces the Id rule now.  
Empty Id matches any instance at this UDP endpoint.  
Non-empty Id must match this instance Id.  
Legacy JSON is not accepted.  
`inhibit-agent`, `inhibit-test`, and the send helpers emit type 18 and require a Controller ID.

### Receive path: always-ephemeral UDP inhibit listener

Terms in this note:

- **UDP inhibit endpoint** = `host:<udp-port>` for type-18 holds.  
- **PTT serial device** = Settings → Radio → Port for RTS/DTR.  

These are not the same thing.

I made the UDP inhibit listener always ephemeral. There is no fixed well-known UDP port.

Reasons:

- One rule for the UDP inhibit endpoint.
- Two WSJT-X instances on one host do not share one UDP port.
- `ShareAddress` on one UDP port did not give reliable per-instance delivery.
- Each instance has its own UDP port, as each instance has its own command endpoint.
- Type 17 (`InhibitStatus`) reports the bound UDP inhibit port on bind and every 15 s.
- WIMS reads that stream and builds the KEY-agent target list (`host:<udp-port>`).
- A standalone `inhibit-agent` can do the same when it gains type-17 discovery.

The gate still uses a separate socket on the transceiver thread next to PTT.  
Type 18 does not go through MessageClient on the GUI thread.  
That keeps latency and isolation.  
The wire format is the same either way.

This update includes:

1. Type 18 with per-controller leases.  
2. An ephemeral UDP inhibit listener. Type 17 announces it on bind, on clear, and every 15 s while TX Inhibit is enabled.  
3. Controllers learn that UDP endpoint from type 17, as they learn the command endpoint from Heartbeats.  
4. No fixed UDP inhibit port. No busy-UDP-port warning path.

### UI: Enable TX Inhibit vs PTT serial device

Enable TX Inhibit is available only when both are true:

- PTT method is RTS or DTR.  
- The PTT serial device (Settings → Radio → Port) is not empty.  

If the operator clears that serial device, the checkbox clears.  
The gate behavior when enabled does not change.  
This only controls when the feature can be armed.  
It stops Inhibit from looking enabled when no RTS/DTR device is set.

A separate PR will cover the PTT/CAT serial device list (custom paths such as `/dev/ttyUSB9700a`, writable line edit, refresh when Settings opens). That PR follows its own timeline. It is Configuration UX, not the inhibit wire.

### InhibitStatus (type 17): when it is sent

Type 17 was already in this PR. The field layout is unchanged.

The send times changed.  
Before, type 17 went out mainly at startup and on hold or badge changes.  
That failed for late joiners and some enable toggles.  
Now type 17 also goes out when the UDP inhibit listener binds or clears, and every 15 s (`NetworkMessage::pulse`) while TX Inhibit is enabled.

With an ephemeral UDP listener, type 17 is how controllers learn the UDP inhibit port.  
Type 17 also carries gate state for triage (held or not, holder, counters).

### Still true

- Opt-in, off by default  
- Lease TTL deadman  
- Inhibit is not Halt Tx (sequencing and audio continue under a hold)  
- RTS/DTR-only enable  
- Never leave RTS asserted on serial open  

I am glad to test a development build from you against this dialect. I will have more time after the September VHF Contest.
