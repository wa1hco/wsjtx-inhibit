# Draft: TX Inhibit update for wsjt-devel

Send from your subscribed address to wsjt-devel@lists.sourceforge.net.
If the first heads-up was never sent, this note stands alone.

---

Subject: TX Inhibit PR #61 updated — three gate seams from Improved review

Hello all,

Follow-up on the TX Inhibit pull request:

    https://github.com/WSJTX/wsjtx/pull/61

The PR is still an opt-in, low-latency PTT hold-off for multi-op
stations that share a band with SSB/CW (W2SZ VHF contest use). Off by
default; sequencing and audio continue under a hold. Based on released
v3.0.2.

A review of the same feature on the WSJT-X Improved port produced three
small gate seams. Those are now on the PR. Hold policy and the UDP
datagram format are unchanged.

1. Settings accept now takes enable-inhibit from the same RTS/DTR
   test as the transceiver, so CAT/VOX cannot look "protected" when
   no gate is running.

2. Closing the rig clears the Inhibit badge. Previously the port was
   zeroed without emitting, so the status line could stick.

3. If rig_set_ptt fails while the gate is driving the pin, that is
   now a transceiver failure (same visibility as stock PTT errors),
   not a logged UDP-bind warning.

Help → About is "About WSJT-X" again; fork branding had leaked into
the first posting.

Still only in the working fork (https://github.com/wa1hco/wsjtx-inhibit),
not in this PR: packaging, and a standalone KEY agent that reads USB-
serial CTS and sends the hold datagrams. Glad to follow up with either
if the team wants them.

Review comments welcome on the PR or here.

73,
Jeff, WA1HCO
