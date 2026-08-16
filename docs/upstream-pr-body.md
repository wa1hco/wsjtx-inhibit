# PR body for WSJTX/wsjtx — copy into the pull request

Title: Add low-latency TX Inhibit for multi-op single-transmitter interlock

---

## Motivation

WSJTX and SSB/CW stations sharing a band — in our case the W2SZ VHF contest group interleaving FT8 with SSB/CW — must keep only one transmitter keyed at a time. We have found that WSJTX can listen through SSB/CW station activity.  The next goal is FT8 working stations while SSB/CW continues to operate with priority. An interlock needs to hold off the radio's PTT key line within milliseconds whenever SSB/CW transmits. 


## Design

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

## Testing

- `tests/test_tx_inhibit_logic.cpp` — QtTest suite for the pure logic.
- `tests/test_tx_inhibit_gate.cpp` — integration suite over a real UDP
  socket and event loop, covering the intent/hold wiring most exposed
  to churn in the upstream-owned files.
- `tools/send_inhibit_hold.py` sends hold/release datagrams for bench
  testing without interlock hardware.
- Field-tested through two release candidates with the W2SZ group,
  including hold/release timing on real radios through RX→Tune→RX
  cycles; CI-built on Linux (x86_64/aarch64), Windows, and macOS in
  the parent fork (https://github.com/wa1hco/wsjtx-inhibit).

The branch is based on the released v3.0.2 (`ccdfaf3`), per the
Programmer's Overview. Happy to split into a review-friendly series,
rename the protocol/UI, or adjust to fit the project's direction.

## Since first posting

A later review of the same feature on the WSJT-X Improved port
produced three small gate seams. They are now on this branch (commit
`2f6d07c`). Hold policy and wire format are unchanged.

- `accept()` takes `enable_tx_inhibit_` from `gather_rig_data()`
  (RTS/DTR only). The raw Settings checkbox could stay true under
  CAT/VOX while no gate was running.
- `close_rig()` now emits `tx_inhibit_changed(false)` so the status
  badge cannot stick after the transceiver is torn down.
- A failed `rig_set_ptt` while the gate is driving the pin is
  `pttApplyFailed` → `Transceiver::failure`, not a logged bind error.

The same commit restores Help → About to "About WSJT-X" (fork
branding that had leaked into the first posting).

Left in the `wa1hco/wsjtx-inhibit` fork, not this PR: packaging /
CI / install docs, the standalone `inhibit-agent` KEY agent (USB-serial
CTS → hold), and the `inhibit-test` keyboard bench tool. Happy to
follow up with any of those if useful.

---

## Notes for you (not part of the PR)

Open it with:

    gh pr create --repo WSJTX/wsjtx --head wa1hco:tx-inhibit \
      --title "Add low-latency TX Inhibit for multi-op single-transmitter interlock" \
      --body-file <(sed -n '/^## Motivation/,/^---$/p' docs/upstream-pr-body.md | sed '$d')

or via the web UI: https://github.com/WSJTX/wsjtx/compare/master...wa1hco:wsjtx:tx-inhibit
