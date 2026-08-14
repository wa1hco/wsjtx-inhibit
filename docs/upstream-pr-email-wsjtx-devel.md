# Draft: TX Inhibit heads-up for wsjt-devel

The team moved to GitHub (June 2026 announcement): code contributions
are now pull requests against WSJTX/wsjtx based on the latest released
code. The PR itself carries the technical detail (see
upstream-pr-body.md); this email is a short heads-up to the list,
where the team watches for discussion. Send from your subscribed
address to wsjt-devel@lists.sourceforge.net.

---

Subject: TX Inhibit for WSJT-X, a single-transmitter interlock — PR opened

Hello all,

Following the team's move to GitHub and the call for contributions on
the new Programmer's Overview page, I've opened a pull request adding
an opt-in, low-latency TX Inhibit function to WSJT-X:

    https://github.com/WSJTX/wsjtx/pull/61

Motivation in brief: WSJT-X and SSB/CW stations sharing a band — in our
case an ARRL VHF contest multi-op — must keep only one transmitter keyed
at a time. Halt Tx is the wrong instrument, since it aborts the QSO
sequence. TX Inhibit instead holds off the radio's PTT key line within
milliseconds while sequencing and audio continue, releasing PTT when the
interlock clears. Off by default; zero behavior change unless enabled.

The PR is based on released v3.0.2, includes two QtTest suites and a
bench-test helper, and has been field-tested through two release
candidates with the W2SZ group. The same feature ported to WSJT-X
Improved has since been exercised on the air, holding off PTT
mid-transmission during a live QSO with sequencing carrying on
underneath. I'm glad to adjust protocol, UI, or structure to fit the
project's direction — review comments welcome on the PR or here.

73,
Jeff, WA1HCO
