List-safe plain text (also docs/n1mm-remote-wsjtx-logging.txt).
To: wsjt-devel, N1MMLoggerPlus, Improved if you wish.

---

To: wsjt-devel@lists.sourceforge.net, N1MMLoggerPlus@groups.io
From: Jeff Millar, WA1HCO
Date: 23 Aug 2026

Subject: WSJT-X cannot log to N1MM on another PC via UDP 2333; TCP 52001 works

We need a way for WSJT-X on PC A to log to N1MM on PC B.

VHF multi-op (W2SZ): we cannot use the usual port-2237 approach. We are
developing WIMS, a way to manage many WSJT-X instances alongside SSB/CW
on the same bands. WIMS handles interlocking and a roster of needed
stations from the decode lists. To do that, many WSJT-X instances use
UDP multicast 2237 to spread decode lists to WIMS and/or GridTracker.
In general, one N1MM per band (used by the SSB/CW operator) is the
logger. If that N1MM also joins 2237, it logs every instance and every
band. So logging cannot ride the decode-group multicast.

WSJT-X Secondary UDP (host:2333, raw ADIF) is the documented unicast
way to log. N1MM still tells operators to use 2333.

UDP 2333 logs from localhost only, not from the LAN.

N1MM binds 0.0.0.0:2333 (N1MMLogger.net.exe). Wireshark on the N1MM PC
sees the datagram from the WSJT-X PC. Sending the same bytes to
127.0.0.1:2333 on the N1MM PC inserts a QSO. Sending them from the
other PC does not -- raw ADIF, JTAlert <command:3>Log ...>, or WSJT-X
binary type 12. Pointing N1MM's WSJT UDP IP at the sender did not
help.

Noted in 2019: ON4ACP, Linux to N1MM on another PC, 2333, packets
arrive, no QSO. N2AMG: works on localhost, had not tried a real IP.
https://groups.io/g/N1MMLoggerPlus/message/44025

TCP 52001 is N1MM's JTDX log socket, and it does accept a LAN client.

N1MM Configurer -> WSJT/JTDX Setup -> "JTDX / Others TCP" (not the
WSJT-X UDP row). N1MM is the TCP server; JTDX is the documented
client. On Log QSO, JTDX connects to N1MM and sends the same
<command:3>Log ...> envelope as 2333. The Configurer says ignore this
row if you are using WSJT-X -- so stock WSJT-X never opens it.

Same PC: N1MM listens on 127.0.0.1:52001. Other PC: N1MM listens on
its own LAN IP (e.g. 192.168.2.167:52001); the WSJT-X/JTDX box is the
client. That is the opposite of 2333: 52001 is the remote-log path
N1MM already documented for JTDX.

Do not confuse this with TCP 52002/52004: those are Commander-style
CAT/PTT when N1MM "Load WSJT-X", not ADIF.

We enabled JTDX/Others TCP on 192.168.2.167:52001, connected from the
WSJT-X PC, sent the Log envelope, got ACK, QSO appeared. (Windows
Firewall: a Public NIC dropped 52001 until the profile was Private.)

  Port            Who talks                 What it carries
  UDP 2237        WSJT-X -> group           Binary: decodes, status, log
  UDP 2333        JTAlert -> N1MM, same PC  Log envelope; LAN source ignored
  TCP 52001       JTDX/helper -> N1MM       Log envelope; LAN client works
  TCP 52002/52004 WSJT-X launched from N1MM CAT/PTT, not a log

Ask

1. N1MM: Is remote 2333 a bug, or localhost-only? If the latter, say
   so in the docs; point remote logging at 52001.

2. WSJT-X: Secondary is still UDP + unwrapped ADIF. A remote N1MM
   needs a short TCP client to n1mm:52001 with the Log envelope
   (JTDX's path, not 52002 CAT).

3. I can patch WSJT-X to use TCP on 52001, ship a helper that converts
   a WSJT-X log message to localhost 2333, or follow an N1MM-side fix.

thoughts?

73,
Jeff, WA1HCO
