# Draft reply to dchristle on WSJTX/wsjtx#61

Status: **draft — do not post until reviewed**  
Review: https://github.com/WSJTX/wsjtx/pull/61 (commented 2026-08-25)  
Working notes: `WIMS-wsjtx-PR61-protocol-review.md`

Copy everything below the horizontal rule into the PR conversation.

---

Hi @dchristle — thank you for the careful review. Agree this is the right time to settle the protocol, before anything third-party grows around the current JSON/22372 shape.

I want to separate three things that got bundled together:

1. **Hold model** — per-controller leases, OR’d  
2. **Wire encoding** — `NetworkMessage` type 18 vs JSON  
3. **Receive path** — existing command bus vs a dedicated inhibit listener  

I accept (1) fully, am happy to move to (2), and want to push back on making (3) the *primary* interlock path.

### Accepting: leases and (likely) type 18

**Per-controller leases, OR’d.** You are right that the current global hold is wrong for more than one sender: any `ttl_ms: 0` clears everyone else’s hold. Receiver-side OR is cheap and matches paralleled KEY lines. I will change the gate to one expiring row per Controller ID; PTT stays blocked while any row is live. A sender may only refresh or release its own row. Deadman stays: missed refresh → that row expires → gate opens when none remain.

**Wire encoding.** Your type 18 sketch looks right:

```
InhibitStatus   Out   17   WSJT-X reports aggregate receiver/hold state
TxInhibit       In    18   Controller creates, refreshes, or releases a hold

TxInhibit       In    18
Id (target WSJT-X instance)  utf8
Controller ID                utf8
TTL ms                       quint32
Station                      utf8
```

Status is already binary `NetworkMessage`. Keeping a second JSON dialect forever is a tax we should not pay. I am willing to make type 18 the published inbound dialect. JSON on 22372 would be transitional only (W2SZ field builds), dual-accept for a window, then dropped.

### Pushing back: dedicated inhibit listener stays first-class

How we actually deploy this at a multi-op (W2SZ-style) matters for the receive-path question.

**Plane A — shared reporting bus (all bands on one multicast):** every WSJT-X instance sends Heartbeat / Status / Decode / QSOLogged to `224.0.0.73:2237`. WIMS joins that one group so operators can subscribe to a decode roster filtered by band. Separately, log agents hear the same contact-complete traffic and forward selected bands to that band’s N1MM. So 2237 is intentionally a **cross-band, many-publisher** bus. It is a poor place to hang a *selective* millisecond interlock: WSJT-X does not receive commands on 2237 anyway (commands go to each instance’s ephemeral source port), and even the discover-then-command pattern has to pick “the digi seats on *this* band” out of an all-bands stream shared with roster and logging.

**Plane B — KEY → digi PTT interlock:** SSB and CW on a band each have a KEY sense. When either keys, **all digi seats on that band** must drop PTT. Digi-vs-digi “only one WSJT-X may TX” is a different control plane (WIMS Enable Tx / Halt / Work on the command bus). We do not mix those.

**Who does selection:** WIMS already owns band membership. Its KEY agent watches SSB/CW KEY and sends hold datagrams to a **short unicast list** of WSJT-X stations on that band (typically ≤3). The list is managed by WIMS. The WSJT-X inhibit gate does **not** need to know about bands, fleets, or N1MM — it only applies holds it receives (and, with your lease model, OR’s per-controller rows).

```text
All bands WSJT-X ──Decode/Status/Log──► 224.0.0.73:2237 ──► WIMS / log agents / N1MM

SSB KEY ──┐                    unicast hold to band digi list (≤3)
          ├──► WIMS key agent ──────────────────────────────────► each digi :22372
CW  KEY ──┘
```

So the fixed-port listener is not “a second app bus.” It is the **dumb PTT filter** at the end of a controller-owned fan-out. The critical path is:

```text
KEY assert → agent → UDP hold → gate → do not assert PTT
```

That path needs to be small, always-on, and independent of the companion ecosystem. A dedicated listener (default **22372**, own socket, short path into the gate) is what we field-tested. My concern with making the **existing command path** the *only* or *primary* receive path:

- **Wrong bus for selection.** All-bands multicast 2237 is for roster/logging. Selective “hold these digi on 222” is already solved by a WIMS-managed unicast list to known gate ports. Putting inhibit on the ephemeral command path re-introduces discovery and all-bands filtering into the KEY path for no gain.
- **Latency / scheduling.** The command bus shares UDP demux with Heartbeat / Status / Decode / HaltTx / Configure, and sits behind “Accept UDP requests” and multicast membership. Inhibit needs milliseconds from KEY to PTT filter.
- **Discovery interdependence.** Clients must learn a **moving** ephemeral port from Heartbeats (startup, then ~15 s). Fine for UI commands. Wrong for an interlock: the agent already has `host:22372` from the WIMS list (or a dual-radio seat config).
- **Failure coupling.** Same path as roster/logging means a flood, blocked “Accept UDP requests,” or companion congestion can disable the interlock. Inhibit ≠ Halt.
- **Agent shape.** Standalone dual-radio seats should stay CTS → one `host:port`, not a mini MessageServer client. WIMS can afford discovery for building the list; the gate still should not depend on that bus to *receive* the hold.

The requirement the fixed port satisfies: **a stable, always-bound inhibit address that a KEY agent (WIMS list or local config) can hit without riding the all-bands companion bus.**

I am **not** arguing for a second *protocol forever*. I am arguing that a **dedicated listener remains first-class** for the KEY→PTT case — preferably speaking the **same** type 18 binary you proposed. Band selection stays in the controller; the gate stays dumb.

### Proposed split

1. Type 18 + per-controller leases as above.

2. **Primary receive path for KEY agents:** dedicated port (default 22372), same type 18 binary. Configured once as `host:22372`. No discovery. Short path into the gate.

3. **Optional secondary:** accept the same type 18 on the existing inbound command endpoint for companions that already speak that bus (GridTracker-class). Useful; not a substitute for (2).

4. JSON on 22372: transitional dual-accept for current field builds, then remove from the published protocol.

5. Keep either way:
   - Opt-in, off by default  
   - TTL / lease deadman  
   - Inhibit ≠ Halt (sequencing and audio continue under a hold)  
   - RTS/DTR-only enable (Improved-review seam)  
   - Never leave RTS asserted on serial open  

I am open to being convinced that the main command path can match the dedicated listener on **latency and isolation** — for example if type 18 can be accepted on a directed `host:port` with no Heartbeat wait, no dependence on “Accept UDP requests,” and a receive path that cannot be starved by companion traffic. If that can be shown, the dedicated port could shrink to optional convenience. Until then I want it as the published KEY-agent path, not as a footnote.

### Deployed controllers that would break on endpoint / wire change

Only first-party so far:

- `wa1hco/wsjtx-inhibit` RCs in use at W2SZ  
- WIMS `wims-key-agent`  
- this PR’s `inhibit-agent` / `inhibit-agent-gui`  

No third-party GridTracker / N1MM inhibit clients that I know of. Best time to change the wire. We can update all three in lockstep.

Happy to take a development build from you for compatibility testing, and/or push a follow-up on this branch that lands leases + type 18 on the dedicated port (JSON dual-accept temporarily) so we have something concrete to bounce packets against.

Thanks again — the lease model in particular is a clear improvement over what shipped in the RCs.
