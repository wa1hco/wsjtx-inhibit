# Results: WS 3.2.1 260926 TX Inhibit pin time

Measured 2026-09-29 on `jeff-Precision-Tower-3420` (Ubuntu, Intel i7-6700, 8 threads) against the local WS 3.2.1 **260926** build, `/home/jeff/ham/ws-3.2.1-260926-build/ws`. Not the official WSJT-X 3.2.0-rc1 AppImage, and not the uncommitted sole-writer bypass in `ws-suite`.

- Rig: Icom IC-7300, CAT `/dev/ttyUSB0` at 115200, PTT method RTS on that same port, Split = Fake It
- Mode: FT8, transmit into a dummy load. The probe checked Enable Tx with a Reply (type 4) to a live decode and reset the six-minute TX watchdog with Free Text every 90 s. Halt Tx at the end of each sweep.
- UDP: `127.0.0.1:2237`, Id `WS`, Accept UDP requests on
- Probe: `tools/probe_tx_inhibit_latency.py --ttl-ms 100 --interval-ms 137`
- Clock: `CLOCK_MONOTONIC`

The inhibit thread clears RTS with `TIOCMBIC` when it reads a type 18. `socket read → RTS` is that thread's stamp at the start of the clear until the ioctl returns. `send → RTS` is the probe's stamp immediately before `sendto` until the same ioctl return. Type 17 arrival is not pin time. During transmit it still comes back about 52 ms later, after the transceiver thread, including the 50 ms sleep at the end of `rig_set_ptt()`.

CSVs:

- [ws321-latency/inhibit-latency-ws321-30min-tx.csv](ws321-latency/inhibit-latency-ws321-30min-tx.csv)
- [ws321-latency/inhibit-latency-ws321-5min-rebuild.csv](ws321-latency/inhibit-latency-ws321-5min-rebuild.csv)

## Idle desktop, 30 minutes

Load average about 1.3. Span 1800 s. 12,643 holds.

| | n | p50 | p99 | max | ≥ 5 ms | ≥ 30 ms |
|---|---:|---:|---:|---:|---:|---:|
| Transmit, socket read → RTS | 5648 | 0.15 ms | 0.48 ms | 3.1 ms | 0 | 0 |
| Transmit, send → RTS | 5648 | 0.33 ms | 0.74 ms | 5.3 ms | 2 | 0 |
| Idle, send → RTS | 6925 | 0.32 ms | 1.1 ms | 9.8 ms | 9 | 0 |
| Decode, send → RTS | 70 | 0.23 ms | 0.61 ms | 0.62 ms | 0 | 0 |

Socket read → RTS is the ioctl after the inhibit thread has the packet. Send → RTS adds the localhost handoff onto that clear. The two transmit holds over 5 ms were 5.1 ms and 5.3 ms, and in both the ioctl itself was under 0.4 ms.

The gap from the 99th percentile to the maximum is a thin tail. On transmit, 45 of 5,648 holds took 1 ms or more. Most of those waited before the inhibit thread entered the clear. A few waited inside the USB ioctl. The slow holds are scattered across the 15 s FT8 period, often two or three within the same second. `udp-dispatch` is an ordinary scheduler thread (nice 0). The CP2102 and the PCM2901 audio codec share USB bus 1 behind one hub, so a few ioctl waits of about 3 ms are that bus.

## Rebuild on all 8 cores, 5 minutes

A separate build directory, `/tmp/ws-load-build`, configured from the same 260926 sources. One `cmake --target clean`, then `cmake --build -j8`. The full rebuild did not finish in five minutes (708 object files when the sweep ended). Load average about 8. The running WS binary was not replaced. Span 300 s. 2,106 holds.

| | n | p50 | p99 | max | ≥ 5 ms | ≥ 30 ms |
|---|---:|---:|---:|---:|---:|---:|
| Transmit, socket read → RTS | 950 | 0.11 ms | 3.5 ms | 40 ms | 3 | 1 |
| Transmit, send → RTS | 950 | 1.9 ms | 8.4 ms | 41 ms | 76 | 1 |
| Idle, send → RTS | 1140 | 1.1 ms | 8.6 ms | 36 ms | 74 | 1 |
| Decode, send → RTS | 16 | 3.0 ms | 6.5 ms | 6.5 ms | 1 | 0 |

564 of 950 transmit holds took at least 1 ms from send to the pin. The ioctl median stayed about 0.1 ms. The 41 ms transmit hold spent 40 ms inside the clear. The 36 ms idle hold spent 36 ms waiting for the inhibit thread to run, and the ioctl was 0.1 ms.

An idle desktop leaves a large margin under 30 ms. A compile that keeps every core busy both raises how often the tail happens and, once, spends the 30 ms.
