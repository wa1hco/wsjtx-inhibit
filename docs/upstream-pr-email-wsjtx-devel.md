# Draft: TX Inhibit update for wsjt-devel

Send from your subscribed address to wsjt-devel@lists.sourceforge.net.
If the first heads-up was never sent, this note stands alone.

---

Subject: TX Inhibit PR #61 updated — standalone KEY agent, no WIMS required

Hello all,

Follow-up on the TX Inhibit pull request:

    https://github.com/WSJTX/wsjtx/pull/61

The PR is still an opt-in, low-latency PTT hold-off for multi-op
stations that share a band with SSB/CW (W2SZ VHF contest use). Off by
default; sequencing and audio continue under a hold. Based on released
v3.0.2.

The first posting was only the gate. That is not enough: something
must see the priority KEY and send the hold datagrams. WIMS already
has that program. The PR now ships a standalone KEY agent so a
dual-radio seat works with only WSJT-X — no WIMS required.

- inhibit-agent (CLI): USB-serial CTS + dest host:port
    inhibit-agent --port /dev/ttyUSB0 --addr 127.0.0.1:22372
- inhibit-agent-gui: dest host:port in the window (default
  127.0.0.1:22372); CTS port auto-picked or --port
- Hang: break-in CW 1.5 × word gap so PTT does not follow dits;
  SSB / continuous KEY releases immediately
- Fail-safe: lost agent or dongle → gate deadman (~600 ms) opens

Qt SerialPort is already a WSJT-X dependency. Wire format is
unchanged. Design: docs/INHIBIT_AGENT.md on the PR.

The Improved-review gate seams from the last update are still there
(RTS/DTR enable, close_rig clears the badge, rig_set_ptt failure is
a transceiver failure). Help → About is "About WSJT-X".

Still only in the working fork (https://github.com/wa1hco/wsjtx-inhibit):
packaging, and a keyboard bench tool. Glad to follow up if useful.

Review comments welcome on the PR or here.

73,
Jeff, WA1HCO
