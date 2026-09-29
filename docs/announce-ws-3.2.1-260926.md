# Draft update — WS suite (3.2.1)

Status: **draft — review before send. Not sent.**

Baseline: WS 3.2.1, build **260926** (inner source `ws.tgz`)
Patch: `contrib/improved-list-submission/tx-inhibit-ws-3.2.1_260926.patch`
Apply notes: `contrib/improved-list-submission/README-ws-3.2.1-260926.md`
SHA-256: `46a4510207afb8fbbc165586f4f3d6cdd699e4eb31c88718d4656d704cf2b4b0`

How this project accepts source changes: there is no public git repository.
Send the patch as an attachment, with this note, to

- the community list `wsjt-x-improved-community@lists.sourceforge.net`
- Uwe, DG2YCB (https://www.qrz.com/db/DG2YCB)

Do not paste the patch into the message. Mail clients rewrite line endings.

---

**Subject:** TX Inhibit updates for WS 3.2.1 260926 - simpler messaging, simpler UI, and faster inhibit

---

Hello Uwe and all,

WS 3.2.1 260926 already has TX Inhibit. It listens for a JSON hold on its own UDP port, 22372, and it clears PTT by calling rig_set_ptt on the transceiver thread. That call takes the Hamlib rig lock, so a CAT command already in progress holds the PTT line up.

This note is an upgrade against that version. It follows some really good improvement to inhibit design that David Christle put in WSJT-X 3.2, and it adds the two changes from me that greatly improve the inhibit timing. 

## What changed with inhibit messages

The Enable TX Inhibit checkbox is gone. Inhibit is automatically armed when the PTT method is RTS or DTR, eliminating the "enable" checkbox. Accept UDP requests still has to be on before inhibit is accepted. Turning that off does not cancel a hold that is already running. The hold keeps the pin off until its TTL.

Inhibit command arrives as message type 18, not JSON. Inhibit uses the same UDP socket already used for other UDP commands. WS-suite sends a type 17 inhibit announcement to the UDP server address in Settings, and the controller sends type 18 back to the source address and port of that traffic, with the same Id and schema. There is no separate inhibit port.

Message Type 17 is the inhibit status. The fields are Supported, Inhibited, the station text, and the four counters. Supported means RTS or DTR for PTT and Accept UDP requests. Type 17 goes out when the state changes and again after each Heartbeat. One unsupported snapshot is sent when Accept UDP requests is turned off.

Holds are per controller and OR-combined. Up to 64 controllers are tracked. Any further controller extends one aggregate hold. A TTL of 0 releases that controller. A TTL from 100 to 30000 ms creates or refreshes the hold.

The status box says INHIBIT while a hold is active. In receive, the background is a light green with "Inhibit". In transmit it is white on red "Inhibit".

## Why the pin path changed

The requirement is for fast response to the WSJT-X to SSB handoff. From the SSB transmitter KEY, reading that signal in software, through sending the packet, across the switched Gigabit Ethernet, and through the inhibit time inside WS, the sum has to stay under 30 ms every time. That is what keeps the transmitter transfer relay from being hot-switched. WSJT-X RF is gone before the relay moves.

There is no measurement that can ensure "every time" across two PCs, two operating systems, and a network. The software goal is a large margin under 30 ms. The hardware design has to tolerate a small number of hot switches.

WSJT-X 3.2-rc1 reads a type 18 datagram on the GUI thread. WS suite 3.2.1 currently reads inhibit as a JSON datagram on its own thread. The GUI thread adds about 2 ms at the median and about 6 ms at the 99th, with a worst measured time of 16 ms. A thread which runs separate from the GUI loop brings the same delay to about 0.2 ms at the median and about 3 ms at the peak.

David Christle's point is to use the same UDP socket for Inhibit and that's a good idea. My inhibit patch initially chose to use a different socket with its own thread to avoid an unknown GUI loop delay. But that introduced UI changes and network complexity. The GUI delay turned out better than I feared, but still adds too much peak latency to be comfortable.

The solution is to use the common UDP socket for inhibit, but to add a UDP message preprocessing thread to read the inhibit datagram (type 18 message) as fast as possible and act on it immediately. Any other datagram types are passed to the GUI. This keeps the UI and network consistent and keeps the low latency of a dedicated thread.

After the inhibit datagram read, stock code calls rig_set_ptt(). That takes the rig lock and waits for whatever CAT command is already running. With Fake It, that command is a burst of split and frequency exchanges on the same thread, on the order of 80 to 95 ms. At 115200 baud, for a run of 422 inhibits an IC-7300 gave a 99th percentile of 76 ms from the socket read to RTS, and six holds were over 30 ms. Lowering the CAT baud rate slows an ordinary CI-V poll. It is not what produced that tail.

This upgrade reads the UDP socket on its own thread. When the datagram is a type 18 hold, that thread clears RTS or DTR before the datagram is queued to the rest of the program. It does not take the rig PTT lock. On Linux this is the modem-line ioctl (TIOCMBIC). On Windows, Hamlib turns the same call, ser_set_rts() or ser_set_dtr(), into EscapeCommFunction() with CLRRTS or CLRDTR. While the hold lasts, the same clear is repeated so a later PTT request cannot leave the line high. rig_set_ptt() still runs afterward, only to update Hamlib's saved PTT state. After ser_set_rts(), rig_set_ptt() sleeps 50 ms. The pin has already dropped. That sleep is not part of the inhibit time, but it does hold the rig lock.

## What was measured

The numbers below are from one PC, localhost, IC-7300, PTT method RTS on the CAT port, CAT 115200, Split = Fake It. The clock is CLOCK_MONOTONIC. The probe stamps immediately before sendto of the type 18. The program stamps the UDP-thread clear: the socket read, then the RTS ioctl. The arrival time of the type 17 is not the pin time. During transmit that status still comes back about 52 ms later, after the transceiver thread, including the 50 ms sleep at the end of rig_set_ptt(). Localhost is not a stand-in for switched Gigabit Ethernet. CTS-to-send was not measured. The tables are in docs/RESULTS-ws-3.2.1-260926-inhibit-latency.md.

Stock 3.2.0-rc1, 422 transmit holds: send to RTS was 2.3 ms at the median and 76 ms at the 99th. The maximum was 98 ms. Six holds were over 30 ms. The long part of that number is the Hamlib thread waiting on the rig mutex. I don't know of any reason for that part to be different on WS suite.

Both changes together, on WSJT-X 3.2.0-rc1, 2,393 transmit holds: send to RTS was 0.32 ms at the median, 0.67 ms at the 99th, and 3.98 ms at the maximum. No transmit sample reached 5 ms. Across 8,359 holds (transmit, idle, and decode) none reached 30 ms. The Fake It stalls are gone from the pin time.

On WS 3.2.1 260926, same station, FT8 into a dummy load, the probe checked Enable Tx with a Reply to a live decode and reset the six-minute TX watchdog itself. Thirty minutes on an otherwise idle machine, 5,648 transmit holds: send to RTS was 0.33 ms at the median, 0.74 ms at the 99th, and 5.3 ms at the maximum. Two holds were over 5 ms. None reached 30 ms. Socket read to RTS on those holds was 0.15 ms at the median, 0.48 ms at the 99th, and 3.1 ms at the maximum. Idle send to RTS, 6,925 holds, was 0.32 ms at the median, 1.1 ms at the 99th, and 9.8 ms at the maximum. Nine were over 5 ms. None reached 30 ms.

The same sweep for five minutes while this PC rebuilt the WS sources on all 8 cores moved the transmit numbers. Of 950 transmit holds, send to RTS was 1.9 ms at the median and 8.4 ms at the 99th. Seventy-six were over 5 ms, and one was 41 ms. One idle hold was 36 ms. The 41 ms transmit hold was inside the serial clear. The 36 ms idle hold was the inhibit thread waiting for a CPU. The rebuild had been cleaned once and was still compiling when the five minutes ended. An idle desktop has a large margin. A loaded one can spend the 30 ms.

A separate USB serial port for RTS or DTR was not measured. Hamlib closes that device except while transmitting, and this patch's fast clear then falls back to the CAT adapter, so a second keyline port is not what these numbers describe. Stock rig_set_ptt() still unkeys that port, behind the rig lock. The bench tool that drives this from the grave key is in the wsjtx-inhibit repository (tools/inhibit-test and tools/send_inhibit_hold.py). It is not part of the attached patch. It binds the UDP server port, reads the Heartbeat and type 17, and sends type 18 back to that source.

## Apply

```text
tar xzf ws-3.2.1_260926.tgz
tar xzf ws-3.2.1/src/ws.tgz
cd ws
patch -p1 --binary < tx-inhibit-ws-3.2.1_260926.patch
```

--binary is required if the patch and the tree differ in CRLF handling.

73,
Jeff
