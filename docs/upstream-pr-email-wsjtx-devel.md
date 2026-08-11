# Draft: TX Inhibit proposal for wsjtx-devel

Send from your subscribed address to wsjtx-devel@lists.sourceforge.net.
Review and edit freely — particularly the personal intro and anything
about your availability for follow-up work.

---

Subject: [RFC/PATCH] Low-latency TX Inhibit for multi-op single-transmitter interlock

Hello all,

I'd like to open a discussion about adding a TX Inhibit function to
WSJT-X, and I have a working, field-tested implementation to propose.

Motivation
----------

Multi-op stations sharing a band — in our case the W2SZ VHF contest
group interleaving FT8 with SSB/CW — must keep only one transmitter
keyed at a time. Halt Tx is the wrong tool for that job: it aborts the
QSO sequence. What an interlock needs is a way to hold off the radio's
PTT key line within milliseconds while FT8/FT4 sequencing and audio
continue untouched, then release PTT the moment the band is free.

Design
------

The change is deliberately small and opt-in (off unless enabled in
Settings; zero behavior change otherwise):

- TxInhibit/TxInhibitLogic.hpp — pure gate logic with injected time.
  The invariant is: assert PTT iff (want_tx and not hold).
- TxInhibit/TxInhibitGate.cpp — binds that logic to a UDP socket
  (default port 22372). Holds arrive as small JSON datagrams carrying
  a TTL, so a lost release packet fails safe: the hold expires.
- The Transceiver do_ptt paths consult the gate before keying, and rig
  backend exceptions are contained so a CAT hiccup cannot leave PTT
  stuck asserted.
- While a hold is active the status bar shows a red TX INHIBITED badge.

Two QtTest suites cover it: one for the pure logic, one exercising the
intent/hold interaction over a real UDP socket and event loop — the
wiring a rebase is most likely to break. tools/send_inhibit_hold.py
sends hold/release datagrams for bench testing without any interlock
hardware, and docs/TX_INHIBIT.md documents the protocol and trust
model (the listener binds localhost-scope UDP; there is no remote
control of TX, only inhibition of it).

Code
----

The feature alone, rebased against mainline v3.0.2:

    git fetch https://github.com/wa1hco/wsjtx-inhibit.git tx-inhibit-upstream

Browse: https://github.com/wa1hco/wsjtx-inhibit/tree/tx-inhibit-upstream

Diffstat: 28 files changed, 2156 insertions(+), 30 deletions(-) —
three new files under TxInhibit/, wiring in the Transceiver classes,
Configuration, and mainwindow, plus tests and one doc. I'm happy to
reformat this as a patch series against whatever branch you prefer, or
adjust protocol/UI details to fit the project's direction.

Field testing
-------------

The parent fork (wsjtx-inhibit) has been through two release
candidates with the W2SZ group, including verification on real
hardware of hold/release timing through RX→Tune→RX cycles and CI
builds on Linux (x86_64/aarch64), Windows, and macOS. Release page:
https://github.com/wa1hco/wsjtx-inhibit/releases

I'd value the group's feedback on whether this is a feature the
project wants, and if so, what changes would make it acceptable.

73,
Jeff, WA1HCO

---

## Notes for you (not part of the email)

- The branch is a single commit (3b7cd64) on top of the mainline
  v3.0.2 baseline commit in this repo (26e8bf5). If reviewers want a
  patch series, split it: (1) TxInhibit core + CMake, (2) Transceiver
  wiring, (3) UI/Configuration, (4) tests + tools + docs.
- `git format-patch 26e8bf5..tx-inhibit-upstream` produces the
  mailable patch if the list prefers inline patches over a fetch URL.
- Excluded from the branch on purpose: fork branding (about dialog,
  window title), packaging renames (wsjtx-inhibit deb/rpm/NSIS), CI
  workflows, Windows KEY-agent tools, and fork-process docs.
- Upstream WSJT-X development happens on SourceForge; if a maintainer
  asks for the branch there rather than GitHub, the same commit can be
  pushed to a SourceForge clone.
