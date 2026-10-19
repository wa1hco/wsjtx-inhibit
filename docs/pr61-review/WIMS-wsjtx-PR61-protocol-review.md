# WSJT-X PR #61 — network protocol review notes

Saved from a Grok chat (2026-09-02) for later desktop use.

- PR: https://github.com/WSJTX/wsjtx/pull/61
- Title: Add low-latency TX Inhibit for multi-op single-transmitter interlock
- Author: wa1hco
- Reviewer: dchristle (2026-08-25)
- Related WIMS repo: https://github.com/wa1hco/WIMS
- Related fork: https://github.com/wa1hco/wsjtx-inhibit
- Status when saved: design review posted; no author reply yet; no line comments

This note is analysis of the **net protocol issue**, not a patch.

---

## What the PR does

Opt-in TX Inhibit so FT8 and SSB/CW can share a band without two PAs keyed at once.

- Halt Tx is the wrong tool: it aborts the QSO sequence.
- Inhibit only filters PTT: `assert PTT iff (want_tx and not hold)`.
- Sequencing and audio continue under a hold; PTT releases when the interlock clears.
- Gate listens on a **fixed UDP port** (default **22372**) for **JSON** datagrams with a **TTL**.
- Lost release packet → TTL expires → fail-safe.
- Standalone `inhibit-agent` / `inhibit-agent-gui` reads USB-serial CTS and sends holds so a dual-radio seat does not need WIMS.
- Outbound telemetry already uses existing `NetworkMessage::InhibitStatus` (type 17).

Field-tested with W2SZ via the wa1hco/wsjtx-inhibit fork.

---

## How WSJT-X already talks to the world

WSJT-X does **not** sit on a well-known command port.

Typical setup:

1. Operator sets **UDP Server** to `224.0.0.73:2237` (or `127.0.0.1:2237`).
2. WSJT-X **sends** Heartbeat / Status / Decode **from a random (ephemeral) source port** to that destination.
3. WIMS, N1MM, GridTracker **listen** on 2237 and learn:  
   “instance `TRAILER-50-A` is at `192.168.1.20:51234`.”
4. To command that instance (`Reply`, `HaltTx`, `Configure`) the companion sends a **binary `NetworkMessage`** **back to that learned address**, not to 2237.

Command path = **discover the moving target, then speak its dialect.**

Reference in-tree: `UDPExamples/MessageServer`.

Heartbeat: one at startup, then about every **15 seconds**.

---

## What this PR added instead

A second, private conversation:

- WSJT-X **binds a fixed port** (default **22372**)
- KEY agent sends a **small JSON** datagram: hold for `ttl_ms`
- `ttl_ms: 0` means release
- Agent is configured once: `127.0.0.1:22372`
- No Heartbeat parsing, no ephemeral-port tracking

This is a **sensor-to-gate** path, not an **app-to-app** path.

---

## Reviewer position (dchristle)

Not a reject. Feature is “interesting and useful.” He wants two protocol choices explained, then a safer hold model.

### 1. Why a second UDP endpoint (22372)?

Existing inbound commands already work. Companions subscribe to the configured multicast/unicast stream, learn each instance’s ephemeral endpoint, send commands there, refresh when new traffic arrives.

### 2. Why JSON instead of NetworkMessage / QDataStream?

PR already uses `NetworkMessage` for outbound `InhibitStatus`. Inbound should match.

Proposed pair:

```
InhibitStatus   Out   17   WSJT-X reports aggregate receiver/hold state
TxInhibit       In    18   Controller creates, refreshes, or releases a hold
```

Minimal type 18:

- Id (target WSJT-X instance) — utf8
- Controller ID — utf8
- TTL ms — quint32
- Station — utf8

Nonzero TTL = create/refresh that controller’s hold. Zero TTL = release that controller’s hold.

### 3. Global hold vs per-controller leases

Current PR: one global hold. Any nonzero TTL refreshes it. Any `ttl_ms: 0` clears it **regardless of who asserted the hold**.

Problem: controller B can release controller A’s hold. PTT can come back until A’s next refresh.

He wants: one expiring hold **per controller**, combined with **logical OR**. A sender can only refresh/release its own row. PTT blocked while any row is live. Same as paralleled hardware KEY lines.

WIMS currently OR’s KEY sources **outside** WSJT-X and sends one logical stream. That is fine for WIMS stations. Standalone users would need another coordinator unless the gate ORs internally. Receiver-side OR is cheap; do it in the published protocol.

### His preference for upstream

- New inbound `NetworkMessage::TxInhibit` on the **existing** endpoint
- Per-controller leases
- He offered a development build for compatibility testing
- Question: any deployed controllers besides this PR and WIMS that would be hard to update if endpoint or wire format changed?

---

## Comparison

| | Official command bus | This PR’s inhibit socket |
|---|---|---|
| Purpose | Operator actions (who to call, halt QSO) | Hardware interlock (don’t key the PA) |
| Address | Changes every WSJT-X launch | Fixed `host:22372` |
| How you find it | Wait for Heartbeat (startup + ~15 s) | Written on the KEY agent command line |
| Payload | Binary `NetworkMessage` | JSON `{ttl_ms,...}` |
| If the sender dies | Depends on the app | TTL ends the hold |
| Who can talk | Anyone who sees the multicast stream | Anyone who can reach 22372 |

---

## What the reviewer is right about

- **Leases / OR.** A second agent or a `ttl=0` on exit must not drop another seat’s hold. Do this even if JSON stays for a while.
- **Controller ID** as its own field. Station name is for humans.
- **Don’t publish two protocols forever.** Status is already binary. A second JSON dialect on a second port is a long-term tax.

---

## What to push back on

The existing command path is built for **operator actions**. Inhibit is a **PTT interlock**.

A KEY line cannot wait 15 seconds to learn a port. A KEY agent that must join `224.0.0.73:2237`, parse Heartbeats, remember `(id → ip:ephemeral)`, and re-resolve every 15 s is no longer a small CTS→UDP program.

Also:

- Same port as GridTracker / N1MM / WIMS roster means a flood or a blocked “Accept UDP requests” can disable the interlock.
- `HaltTx` must stay a different message. Sharing **transport** is OK; sharing **Halt semantics** is not.
- Discovery delay is seconds. SSB KEY → PTT drop is milliseconds.

---

## Suggested stance for the PR reply

Accept the **protocol family** and the **lease model**. Keep a **fixed, always-on inhibit listener** as an option, or prove the main UDP path can be addressed with no Heartbeat wait when the sender already knows host:port.

Proposed split:

1. Inbound `NetworkMessage::TxInhibit` (type 18) as sketched. TTL>0 refreshes that controller; TTL=0 releases only that controller; hold = OR of live leases.

2. Where it is received:
   - Upstream-preferred: same socket as other inbound commands, processable with no Heartbeat wait if sender already knows host:port.
   - Keep **optional dedicated port** (default 22372) that accepts the **same binary type 18** (not a second JSON dialect). Dedicated port = KEY agent with no discovery. Main path = GridTracker-class apps.

3. JSON on 22372: **transitional** for W2SZ / current `inhibit-agent` field builds. Dual-accept window, then drop JSON as the published protocol.

4. Deployed controllers that would break (answer his question):
   - wa1hco/wsjtx-inhibit RCs at W2SZ
   - WIMS `wims-key-agent`
   - this PR’s `inhibit-agent` / `inhibit-agent-gui`
   All first-party. Can change in lockstep. No third-party GT/N1MM inhibit clients yet. Best time to change the wire.

5. Do not give up:
   - Opt-in, off by default
   - TTL deadman
   - Process death → no keepalive → gate opens
   - Inhibit ≠ Halt (sequencing continues)
   - RTS/DTR-only enable (Improved-review seam)
   - Never leave RTS asserted on serial open

---

## One-sentence summary

WSJT-X already has a command protocol that finds each instance by watching Heartbeats and speaking binary messages at a moving port. This PR added a second, fixed-port JSON interlock so a KEY dongle can drop PTT in milliseconds with no discovery. The review says: put inhibit on the existing bus (new message type + per-sender leases), and only then argue whether a fixed port is still needed for the dumb KEY agent.

---

## Suggested landing place

`docs/plan/wsjtx-pr61-protocol.md` in the WIMS repo, or keep this file next to the earlier `WIMS-docs-cleanup-2237.md` notes.

Not design (`wims_design.md`) and not build status (`wims_status.md`). This is an upstream-protocol negotiation note.
