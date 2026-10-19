# TX Inhibit latency: components from the RTS ioctl

The requirement covers the whole WSJT-X-to-SSB handoff. From the CTS input that senses the other transmitter, through building and sending the inhibit packet, transport across the switched Gigabit Ethernet, and the inhibit time measured here, the sum must stay under 30 ms at the 99.99th percentile. That keeps the transmitter transfer relay from being hot-switched: WSJT-X RF is gone before the relay moves. The 99.99th percentile was not measured. A delay that rare needs on the order of 10,000 transmit holds. The largest transmit set here is 2,393 holds.

This report measures only the last term, from the packet send to the RTS ioctl, and it measures that term on localhost. CTS-to-send and the switched-GigE hop are not in these numbers. Localhost delivery is microseconds, so it is not a stand-in for the GigE hop.

The clock is `CLOCK_MONOTONIC` on one PC. The probe stamps immediately before `sendto()` of a type 18 hold. WSJT-X stamps when `readDatagram` returns, and again when the RTS ioctl returns. Those two WSJT-X stamps are carried in the status datagram so the probe can subtract them. The time at which that status datagram arrives is not used.

Holds were 100 ms long, sent about every 137 ms, to the heartbeat source. The radio was an IC-7300, PTT method RTS, on the CAT serial port. Split mode was Fake It. The 3.2-rc1 runs used the instance `-r 32rc1-latency`. One run used the inhibit build, whose socket is read on a dedicated thread. Samples below are transmit only.

## The two components

**Send to socket read.** Time from the probe's `sendto()` until WSJT-X has the datagram. On stock 3.2-rc1 that read runs on the GUI thread. On the inhibit build it runs on a thread whose only job is this socket. This piece is that thread reaching the socket. Localhost delivery itself is microseconds. The probe binds port 2237. WSJT-X does not. It receives the hold on the ephemeral port it uses when sending toward 2237.

**Socket read to RTS.** Time from `readDatagram` returning until the RTS ioctl returns. This is the software path after the datagram is in hand: the inhibit check, and, on the stock path, any CI-V command already running on the transceiver thread.

**Send to RTS** is the sum of those two. It is the inhibit term in the 30 ms budget. The CTS-to-send time and the switched-GigE hop still have to fit in what remains.

## Transmit results

Times are milliseconds.

### Stock 3.2-rc1, CAT 19200, ioctl on the transceiver thread

92 holds.

| Component | p50 | p95 | p99 | max | over 30 ms |
|---|---:|---:|---:|---:|---:|
| Send to socket read | 2.08 | 4.70 | 6.58 | 6.71 | 0 |
| Socket read to RTS | 0.31 | 4.75 | 8.83 | 24.72 | 0 |
| Send to RTS | 2.50 | 7.17 | 10.81 | 26.74 | 0 |

File: `inhibit-latency-32rc1-stamp.csv`

### Stock 3.2-rc1, CAT 115200, ioctl on the transceiver thread

422 holds.

| Component | p50 | p95 | p99 | max | over 30 ms |
|---|---:|---:|---:|---:|---:|
| Send to socket read | 1.97 | 4.14 | 5.78 | 10.21 | 0 |
| Socket read to RTS | 0.30 | 0.70 | 75.58 | 94.48 | 6 |
| Send to RTS | 2.32 | 5.19 | 75.77 | 97.64 | 6 |

Six socket-to-RTS samples were 60–94 ms. One more was 5.4 ms. After the first, the slow samples are 30.00 seconds apart. Two further stalls of 96 ms fell just outside the transmit window, on the same 30-second grid. That grid is every other FT8 period.

The maximum socket-read-to-RTS is higher in the 115200 run because more samples were taken, so it was more likely to hit the Fake It burst at an unfavorable time. The key result is that CAT message time drives the RTS timing, and that is the problem.

File: `inhibit-latency-115200b.csv`

### 3.2-rc1 with the RTS ioctl on the thread that read the datagram, CAT 115200

1,659 holds. This is not the stock path. The ioctl runs as soon as the type 18 has been parsed, and it does not take the Hamlib rig lock. `rig_set_ptt` still runs later to update Hamlib's PTT state.

| Component | p50 | p95 | p99 | max | over 30 ms |
|---|---:|---:|---:|---:|---:|
| Send to socket read | 1.92 | 4.42 | 6.30 | 16.38 | 0 |
| Socket read to RTS | 0.17 | 0.42 | 0.52 | 3.41 | 0 |
| Send to RTS | 2.11 | 4.67 | 6.53 | 19.79 | 0 |

No socket-to-RTS sample reached 5 ms. Three send-to-RTS samples exceeded 10 ms. The slowest send-to-RTS sample was 19.8 ms. The 30-second stalls are absent.

This shows a dramatic improvement in socket-read-to-RTS timing that is not affected by CAT messaging.

File: `inhibit-latency-direct-long.csv`

### Inhibit build, dedicated socket thread, CAT 19200, ioctl still on the transceiver thread

383 holds. The heartbeat id is `WSJT-X`. A thread owned by WSJT-X reads the heartbeat socket, so the datagram does not wait for the GUI event loop. The RTS ioctl still runs later, on the transceiver thread, inside the same place stock 3.2-rc1 clears the line. This is not the direct-ioctl path, and the CAT rate is 19200 rather than 115200.

| Component | p50 | p95 | p99 | max | over 30 ms |
|---|---:|---:|---:|---:|---:|
| Send to socket read | 0.19 | 0.24 | 0.27 | 2.87 | 0 |
| Socket read to RTS | 0.33 | 0.60 | 7.18 | 18.00 | 0 |
| Send to RTS | 0.53 | 0.81 | 7.32 | 18.19 | 0 |

Eight socket-to-RTS samples were over 5 ms. None were over 30 ms. The receive thread is what changes the first row: the GUI read of about 2 ms at the median, and about 6 ms at the 99th, falls to 0.19 ms and 0.27 ms. The 18 ms maximum is entirely in the second row. The ioctl was still waiting on the transceiver thread.

This shows the benefit of avoiding the GUI event loop.

File: `inhibit-latency-stamp2.csv`

### 3.2-rc1 with both changes, CAT 115200

2,393 holds. This build reads the heartbeat socket on a dedicated thread and clears RTS with the ioctl on that same thread, before the datagram is handed to the rest of WSJT-X. The Fake It CAT sequence still runs afterward, on the transceiver thread, and updates Hamlib's PTT state. It does not sit in front of the pin.

| Component | p50 | p95 | p99 | max | over 30 ms |
|---|---:|---:|---:|---:|---:|
| Send to socket read | 0.16 | 0.20 | 0.24 | 3.64 | 0 |
| Socket read to RTS | 0.15 | 0.34 | 0.44 | 2.74 | 0 |
| Send to RTS | 0.32 | 0.50 | 0.67 | 3.98 | 0 |

No transmit sample reached 5 ms in any component. The idle set was 5,661 holds and the decode set was 291. Across all 8,359 holds, in every window, nothing reached 30 ms. The 30-second Fake It stalls are absent. The socket read matches the inhibit-build receive thread (0.16 ms at the median instead of about 2 ms), and the ioctl matches the direct-RTS run (0.44 ms at the 99th instead of 76 ms).

Making both changes improves the inhibit response time even further.

File: `inhibit-latency-both-long.csv`

## What each component is

**Send to socket read stays near 2 ms** when the GUI thread does the read. The 95th percentile of that piece is about 4–5 ms. The worst sample in the long direct-ioctl run was 16 ms. The GUI thread is turning over on that scale. The audio device pulls samples on its own thread, so a transmit period does not freeze the GUI for the length of an audio buffer. Reading the same socket on a dedicated thread, in the inhibit build, brings this piece down to 0.19 ms at the median and 0.27 ms at the 99th. That does not shorten the work after the read.

**Socket read to RTS is a few tenths of a millisecond** when the transceiver thread is free. On the stock path the tail is a CAT sequence already running on that thread.

At 19200 the tail of 92 holds reached 25 ms and did not cross 30 ms. That sample count reaches about the 99th percentile, not the 99.99th. A single CI-V round trip at 19200 is large enough to consume most of a 30 ms relay budget. A retry or a Hamlib timeout can pass 30 ms, and 92 trials will usually miss an event that rare.

At 115200 a normal CI-V round trip is only a few milliseconds, and the 95th percentile of socket-to-RTS fell to 0.70 ms. The 99th percentile was still 76 ms because of six holds between 60 and 94 ms. This station's split mode is Fake It. `setXIT` calls `transceiver_tx_frequency`, which runs `HamlibTransceiver::do_tx_frequency` on the same thread that clears RTS. That function sends a sequence and waits for each reply: `rig_set_split_vfo`, the frequency and mode commands (`rig_set_freq`, or `rig_set_split_freq` with `rig_get_mode` and `rig_set_split_mode`), and `rig_set_split_vfo` again. At the end of the slot the restore does the same kind of work. The whole sequence is the 80–95 ms stall. A hold that arrives during it waits for the last reply. The restore often happens after WSJT-X has stopped reporting `Transmitting`, which is why some of those stalls were labeled idle. The ordinary 500 ms SWR and VFO poll is not this tail.

**Moving the ioctl off that thread removes the CAT burst from the pin time.** `ser_set_rts` is a modem-control ioctl. CW keying on RTS or DTR already uses it directly. `rig_set_ptt` is the call that takes the rig lock and waits for the CI-V sequence. In the long run the socket-to-RTS 99th percentile was 0.52 ms and the maximum was 3.4 ms, across enough 30-second Fake It boundaries to have shown the old stall if it were still in front of the pin. Hamlib is told the line is off after the CAT sequence returns, so a later poll does not turn RTS back on.

Send-to-RTS on that path is still dominated by the GUI read. Its 99th percentile was 6.5 ms and its maximum was 19.8 ms, all of it before `readDatagram` returned.

## Recommendation

Clear RTS or DTR with the ioctl on the thread that has parsed the type 18 datagram. Do not take the rig lock to do it. Update Hamlib's PTT state when the lock is free. A dedicated thread for the socket read removes the roughly 2 ms GUI piece, as the inhibit-build run shows, and leaves the Fake It sequence in front of the pin if the ioctl stays on the transceiver thread. Both changes are required. The receive thread alone left an 18 ms maximum. The direct ioctl alone left the GUI read, whose maximum in the long run was 16 ms before `readDatagram` returned.

Raising the CAT rate to 115200 shrinks an ordinary in-flight command. It does not shrink the Fake It sequence, which is what crossed 30 ms in the 422-hold run. A second serial adapter for the PTT line was not tested. Calling `rig_set_ptt` on the transceiver thread would still wait on the rig lock even if the PTT file were a different port.

One Hamlib behavior is not part of this latency. After `ser_set_rts` clears the line, `rig_set_ptt` sleeps 50 ms before it returns. The pin has already dropped. The sleep holds the rig lock. It should not be counted as the inhibit delay.

The 99.99th percentile of the full path was not measured, and CTS-to-send plus the GigE hop were not measured here. The stock path at 115200 already spends the entire 30 ms on the inhibit term alone at the 99th percentile of 422 transmit holds, because the Fake It CAT sequence sits in front of RTS. With both changes together, 2,393 transmit holds stayed under 4 ms from the packet send to the RTS ioctl, and the 99th percentile of that inhibit term was 0.67 ms. That leaves the rest of the 30 ms for the keyline sense, the packet build, and the switched GigE.
