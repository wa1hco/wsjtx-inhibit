# Results: WSJT-X 3.2.0-rc1 TX Inhibit latency (radio PC)

Measured 2026-09-25 on `jeff-Precision-Tower-3420` (Ubuntu, kernel 7.0.0-31-generic) against **official** WSJT-X **3.2.0-rc1** AppImage, isolated config `-r 32rc1-latency`. Not the `wa1hco/wsjtx-inhibit` fork.

- Rig: Icom IC-7300, CAT `/dev/ttyUSB0` @ 19200, **PTT method RTS** on the same port
- Audio: Burr-Brown USB CODEC (SignaLink)
- Mode: live **20 m FT8** 14.074 MHz, Monitor on, band busy (ALL.TXT filling)
- UDP Server: `127.0.0.1:2237`, Accept UDP requests on
- Type 17 `Supported=true` for the whole software-path run
- Probe: `tools/probe_tx_inhibit_latency.py --auto --n 20`
- Hardware RTS tap: not available

CSV: [inhibit-latency-32rc1.csv](inhibit-latency-32rc1.csv) (copy also at `/home/jeff/ham/wsjtx-32rc1-latency/inhibit-latency.csv`)

Times are type-18 send → first type-17 `Inhibited=true` (`lat_status_us`). That is GUI-thread telemetry, a lower bound on “command made it through the GUI.”

## Latency (µs)

| Window | n | min | p50 | p95 | max |
|---|---:|---:|---:|---:|---:|
| idle | 20 | 818 | 1603 | 2823 | 2843 |
| decode burst | 20 | 1539 | 8677 | 15926 | 18640 |
| mid-TX | 0 | — | — | — | — |
| mid-TX unkey (`lat_unkey_us`) | 0 | — | — | — | — |
| RTS edge | 0 | — | — | — | — |

Idle ~0.8–2.8 ms. Decode p50 8.7 ms, p95 **15.9 ms**, max 18.6 ms.

## Mid-TX window

Enable Tx, Tune, and a type-9 Free Text “send now” never produced Status `Transmitting=true`, so the probe never classified a hold as `tx`. Copied operator settings still had **SuperFox mode** and contest leftovers; this station’s live PTT is CAT, and the test used RTS for `Supported`. No on-air FT8 TX and no confirmed unkey. Type-17 during RX still answered holds.

## Budget

W2SZ wants ~30 ms KEY-sense → FT8 PTT release, with ~5–10 ms left for the KEY agent. The WSJT-X hop should stay well under **20 ms p95**.

On this PC, idle is fine (~1.6 ms p50). Live 20 m decode raises type-17 to **16 ms p95**. That is still under 20 ms, and it is several times idle, so GUI-thread receive is the load-sensitive hop. A worse decode pile-up can spend the rest of the 30 ms budget before the KEY agent runs.

3.2-rc1 on this radio PC is inside the 20 ms type-17 p95 limit under this 20 m load, with little margin.

## Later pin-time runs

The type-17 times above are not the RTS clear. Later runs on WS 3.2.1 260926 stamp the UDP-thread ioctl. A 30-minute idle transmit run and a 5-minute run during an 8-core rebuild are in [RESULTS-ws-3.2.1-260926-inhibit-latency.md](../RESULTS-ws-3.2.1-260926-inhibit-latency.md). On the idle run, transmit send-to-RTS stayed under 30 ms (p99 0.74 ms, max 5.3 ms, n=5648). Under the rebuild, one transmit hold reached 41 ms.
