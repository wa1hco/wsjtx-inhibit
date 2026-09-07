# Review: proposed `tx-inhibit` PR update

Date: 2026-09-07  
Scope: PR tip `34a1b05` → then-`main` `59038a01` (proposed update for `wa1hco:tx-inhibit` / [WSJTX/wsjtx#61](https://github.com/WSJTX/wsjtx/pull/61))  
Follow-up resolutions tracked below against later commits on `main` / `settings-serial-port-ux`.

Related: [follow-up-dchristle.md](follow-up-dchristle.md)

---

## Summary (original)

The update correctly replaces the single global JSON hold with `NetworkMessage::TxInhibit` (type 18) per-controller leases, always-ephemeral inhibit bind, instance Id matching via `QCoreApplication::applicationName()`, and periodic type-17 `InhibitStatus` announces on the UDP Server path. GateLogic unit tests cover the important lease OR / Id / deadman cases, and keeping type 18 on the transceiver-thread socket (not MessageClient) remains coherent. Dominant risks were operator/agent DX around the removed fixed port (GUI agent can start with `dest_port == 0`; INSTALL still documents 22372), plus protocol-comment and comment-quality nits rather than gate algorithm defects.

## Resolution status (2026-09-07)

| # | Severity | Topic | Status |
|---|----------|--------|--------|
| 1 | bug | Agent GUI/`start` with `dest_port == 0` | **Fixed** on `main` (`5ca8789`) — NEED GATE; `start`/`send_packet` refuse port 0 |
| 2 | bug | INSTALL* still teach 22372 | **Fixed** on `main` (`5ca8789`) — ephemeral / type 17 wording |
| 3 | suggestion | Type-17 “counter changes” overclaim | **Fixed** — TX_INHIBIT.md + NetworkMessage.hpp cadence wording |
| 4 | suggestion | MainWindow design-history comments | **Fixed** — shortened WHY comments |
| 5 | suggestion | No type-17 discovery in standalone agent | **Open** — NEED GATE makes limitation loud; discovery still TODO |
| 6 | suggestion | Gate integration test gaps | **Open** |
| 7 | suggestion | `inhibit-sim` in Settings for upstream | **Open** — policy for upstream-facing branch |
| 8 | nit | Tester README `--port 22372` | **Fixed** — placeholder port + type 17 note |
| — | (related UX) | Custom PTT path typing / list refresh | **Fixed on branch** `settings-serial-port-ux` (`adcafe17`) — separate PR from protocol |

Inhibit-specific Settings guard (**Enable TX Inhibit** only with non-empty RTS/DTR Port) landed on `main` with issue 1 (`5ca8789`).

---

## Issues (original detail)

### Issue 1 -- Severity: bug
- File: tools/inhibit-agent/agent.cpp:200
- Description: `InhibitAgent::start()` no longer has a default inhibit port (`Config::dest_port` defaults to 0) but did not reject an unset port. `inhibit-agent-gui` called `start_agent()` at startup with an empty Gate field, so a KEY assert could `writeDatagram(..., dest_port=0)` and never reach the ephemeral gate.
- Suggestion: Fail `start()` (and GUI auto-start) when `dest_port == 0`, requiring Apply with a type-17/tooltip port first; optionally refuse `send_packet` while unset.
- Status: **fixed** (`5ca8789` — NEED GATE UI; `start`/`send_packet` guards)

### Issue 2 -- Severity: bug
- File: INSTALL.md:143
- Description: Operator install docs still shipped the old fixed-port dialect (`22372` examples). Authority `docs/TX_INHIBIT.md` and the gate always bind ephemeral and require the type-17/tooltip port, so following INSTALL silently failed holds.
- Suggestion: Rewrite INSTALL* to match TX_INHIBIT.md: no fixed port; copy host:port from InhibitStatus / status-bar tooltip.
- Status: **fixed** (`5ca8789`)

### Issue 3 -- Severity: suggestion
- File: Network/NetworkMessage.hpp:545
- Description: Protocol comment (and `docs/TX_INHIBIT.md` cadence text) says InhibitStatus is emitted on “hold/badge/counter changes”, but `TxInhibitGate::emit_state_if_changed` only emits when inhibited level or badge text changes. Keepalives / invalid-only counter bumps do not trigger an immediate type 17; counters update on the next pulse or next level/badge transition.
- Suggestion: Narrow the wording to hold/badge/port bind-clear + periodic pulse, or also emit when counters change if live triage without waiting 15 s is intended.
- Status: **fixed** (docs + protocol comment)

### Issue 4 -- Severity: suggestion
- File: widgets/mainwindow.cpp (port_changed / tooltip comments)
- Description: Comments narrate prior broken behavior / stale close_rig claim.
- Suggestion: Short WHY only.
- Status: **fixed**

### Issue 5 -- Severity: suggestion
- File: tools/inhibit-agent/
- Description: Always-ephemeral listen ports require every standalone controller to be given an explicit port; agent does not consume type 17 itself.
- Suggestion: Type-17 discovery later, or keep NEED GATE loud until then.
- Status: open (mitigated by NEED GATE; discovery still TODO)

### Issue 6 -- Severity: suggestion
- File: tests/test_tx_inhibit_gate.cpp
- Description: Gaps: Id match at socket boundary; multi-controller OR through real gate; type-17 announce path.
- Suggestion: Add focused tests.
- Status: open

### Issue 7 -- Severity: suggestion
- File: Configuration.cpp (`inhibit-sim`)
- Description: Synthetic PTT port in Settings may read as fork-only scaffolding upstream.
- Suggestion: Local-only define or clear “test scaffolding” docs for upstream branch.
- Status: open

### Issue 8 -- Severity: nit
- File: tools/README-INHIBIT-TESTER.md
- Description: Example still used `--port 22372`.
- Suggestion: Placeholder ephemeral port + InhibitStatus.
- Status: **fixed**

### Issue 1 (agent) -- Status: **fixed**
### Issue 2 (INSTALL*) -- Status: **fixed**
