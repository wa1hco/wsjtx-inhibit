# PR body for WSJTX/wsjtx — copy into the pull request

Title: Add low-latency TX Inhibit for multi-op single-transmitter interlock

---

## Motivation

WSJT-X and SSB/CW stations sharing a band — in our case the W2SZ VHF
contest group interleaving FT8 with SSB/CW — must keep only one
transmitter keyed at a time. We have found that WSJT-X can listen
through SSB/CW station activity. The next goal is FT8 working stations
while SSB/CW continues to operate with priority. An interlock needs to
hold off the radio's PTT key line within milliseconds whenever SSB/CW
transmits.

Halt Tx is the wrong tool: it aborts the QSO sequence. TX Inhibit only
filters whether to assert PTT. Sequencing and audio continue under a
hold, and PTT is released the moment the interlock clears.

## Design

Two pieces, both in this PR.

### 1. PTT gate (this program)

Opt-in and off by default — zero behavior change unless enabled in
Settings.

- `TxInhibit/TxInhibitLogic.hpp` — pure gate logic with injected time.
  Invariant: assert PTT iff (want_tx and not hold).
- `TxInhibit/TxInhibitGate.cpp` — binds the logic to a UDP socket
  (default port 22372). Holds arrive as small JSON datagrams carrying
  a TTL, so a lost release packet fails safe: the hold expires.
- Transceiver `do_ptt` paths consult the gate before keying; rig
  backend exceptions are contained so a CAT hiccup cannot leave PTT
  stuck asserted.
- While a hold is active the status bar shows a red TX INHIBITED badge.

There is deliberately no remote control of TX — only inhibition of it.
`docs/TX_INHIBIT.md` documents the datagram format and trust model.

### 2. Standalone KEY agent (`inhibit-agent` / `inhibit-agent-gui`)

The gate is not useful by itself. Something has to see the priority
KEY and send the hold datagrams. WIMS already has that program
(`wims-key-agent`; destinations come from WIMS discovery). This PR
ships a standalone agent so a dual-radio seat works with only WSJT-X —
no WIMS required.

- USB-serial **CTS** is the KEY input. The operator supplies the port.
- Destination is the WSJT-X gate `host:port` (default `127.0.0.1:22372`).
- Hang policy matches the design doc: break-in CW hangs 1.5 × word gap
  so WSJT-X PTT does not follow dits; SSB / continuous KEY releases
  immediately. No software PTT debounce — the sense path is for
  switches that already debounce.
- **CLI** (`inhibit-agent`) for scripts, SSH, and startup files:
  `inhibit-agent --port /dev/ttyUSB0 --addr 127.0.0.1:22372`
- **GUI** (`inhibit-agent-gui`) for operators: dest `host:port` is
  editable in the window; CTS port is auto-picked (Keyline / WA1HCO
  USB strings, else the only non-builtin USB-serial) or `--port`.
- Fail-safe: process exit or a missing dongle stops keepalives; the
  gate deadman (~600 ms) opens. Wrong or missing COM is SENSE FAULT,
  never silent protection.

`docs/INHIBIT_AGENT.md` is the agent design. Wire format is the same
as any other KEY agent. Qt SerialPort is already a WSJT-X dependency.

## Testing

- `tests/test_tx_inhibit_logic.cpp` — QtTest suite for the pure logic.
- `tests/test_tx_inhibit_gate.cpp` — integration suite over a real UDP
  socket and event loop, covering the intent/hold wiring most exposed
  to churn in the upstream-owned files.
- `tools/inhibit-agent/test_keying_monitor.cpp` — hang-policy checks
  (SSB hang 0; break-in hang sizing) with no serial and no UDP.
- `tools/send_inhibit_hold.py` sends hold/release datagrams for bench
  testing without a KEY dongle.
- Field-tested through two release candidates with the W2SZ group,
  including hold/release timing on real radios through RX→Tune→RX
  cycles; CI-built on Linux (x86_64/aarch64), Windows, and macOS in
  the parent fork (https://github.com/wa1hco/wsjtx-inhibit).

The branch is based on the released v3.0.2 (`ccdfaf3`), per the
Programmer's Overview. Happy to split into a review-friendly series,
rename the protocol/UI, or adjust to fit the project's direction.

## Since first posting

### Gate seams from the Improved review (commit `2f6d07c`)

A later review of the same feature on the WSJT-X Improved port
produced three small gate seams. Hold policy and wire format are
unchanged.

- `accept()` takes `enable_tx_inhibit_` from `gather_rig_data()`
  (RTS/DTR only). The raw Settings checkbox could stay true under
  CAT/VOX while no gate was running.
- `close_rig()` now emits `tx_inhibit_changed(false)` so the status
  badge cannot stick after the transceiver is torn down.
- A failed `rig_set_ptt` while the gate is driving the pin is
  `pttApplyFailed` → `Transceiver::failure`, not a logged bind error.

The same commit restores Help → About to "About WSJT-X" (fork
branding that had leaked into the first posting).

### KEY agent now on this PR

The first posting left the KEY agent in the working fork. That was
wrong: without an agent the gate only works if the operator already
has WIMS (or writes their own sender). `inhibit-agent` and
`inhibit-agent-gui` are now on this branch so TX Inhibit is usable
on a dual-radio seat that is not a WIMS station.

Still only in the `wa1hco/wsjtx-inhibit` fork, not this PR: packaging
/ CI / install docs, and the keyboard bench tool `inhibit-test`.

---

## Notes for you (not part of the PR)

Update the live description with:

    gh pr edit 61 --repo WSJTX/wsjtx \
      --body-file <(sed -n '/^## Motivation/,/^---$/p' docs/upstream-pr-body.md | sed '$d')

or via the web UI: https://github.com/WSJTX/wsjtx/pull/61
