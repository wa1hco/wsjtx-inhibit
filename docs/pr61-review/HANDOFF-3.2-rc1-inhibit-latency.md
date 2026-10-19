# Handoff: measure WSJT-X 3.2-rc1 TX Inhibit latency on the radio PC

You are a Grok instance on the PC next to the radio. Jeff (WA1HCO) left this note in `wa1hco/wsjtx-inhibit`. Your job is to run **official WSJT-X 3.2.0-rc1** on live 20 m FT8 and measure how long a type-18 TX Inhibit hold takes to drop PTT while the GUI is actually decoding.

Do the work. Do not wait for another design round.

Repo: https://github.com/wa1hco/wsjtx-inhibit

| File | GitHub |
|---|---|
| This note | https://github.com/wa1hco/wsjtx-inhibit/blob/main/docs/pr61-review/HANDOFF-3.2-rc1-inhibit-latency.md |
| Probe script | https://github.com/wa1hco/wsjtx-inhibit/blob/main/tools/probe_tx_inhibit_latency.py |

On the radio PC:

```text
curl -LO https://raw.githubusercontent.com/wa1hco/wsjtx-inhibit/main/docs/pr61-review/HANDOFF-3.2-rc1-inhibit-latency.md
curl -LO https://raw.githubusercontent.com/wa1hco/wsjtx-inhibit/main/tools/probe_tx_inhibit_latency.py
```

Python 3, stdlib only. Windows: `python` instead of `python3` if that is what is on PATH.

---

## Why this measurement exists

W2SZ keys an SSB/CW radio and needs the FT8 radio’s PTT released in about **30 ms**.

3.2-rc1 receives TX Inhibit on the **GUI thread** (`MessageClient`), then queues the hold to the transceiver thread. A busy GUI (waterfall + Band Activity at the end of an FT8 period) can delay the UDP read. PR #61 on `WSJTX/wsjtx` put the inhibit socket on the transceiver thread for that reason.

Linux already measured a **synthetic** GUI stall (tight loop, no `processEvents`):

| Path | Load | p50 hold → PTT-gate |
|---|---|---|
| 3.2 production (GUI `MessageClient`) | idle | 1.3 ms |
| 3.2 production | GUI busy 20 ms | 21.2 ms |
| 3.2 production | GUI busy 50 ms | 51.2 ms |
| Dedicated UDP on transceiver thread | idle / busy 50 ms | ~0.01–0.1 ms |

That synthetic stall is a busy-wait, not a real decode. You are here to measure the live path: type-18 send → PTT/RTS drop (and type-17 `Inhibited`) during 20 m FT8 decode and during TX.

---

## What 3.2-rc1 actually does

Production receive path:

1. UDP datagram arrives on `MessageClient` (GUI thread, same socket as Heartbeat/Status/Decode).
2. `parse_tx_inhibit` emits `tx_inhibit_command`.
3. `Configuration` forwards it to `TxInhibitTransceiver` with `Qt::QueuedConnection`.
4. Transceiver thread ORs per-controller leases and forces PTT/Tune off on the wrapped rig.

Arming (no separate “Enable TX Inhibit” checkbox):

- Settings → Radio → **PTT Method = RTS or DTR**, with a real serial port.
- Settings → Reporting → **Accept UDP requests** checked.
- Type 17 `InhibitStatus.Supported` becomes true.

Command address:

- Learn the **source IP and source port** of ordinary WSJT-X traffic (Heartbeat is enough).
- Send type 18 to that endpoint, with the **exact Id** from Heartbeat, schema 2 or 3.
- TTL 100–30000 ms creates/refreshes a lease; TTL 0 releases that controller only.

---

## What to run on this PC

Use the public 3.2.0-rc1 installer, not the `wa1hco/wsjtx-inhibit` fork.

- Release: https://github.com/WSJTX/wsjtx/releases/tag/v3.2.0-rc1
- Windows: https://github.com/WSJTX/wsjtx/releases/download/v3.2.0-rc1/wsjtx-3.2.0-rc1-win64.exe

---

## WSJT-X settings for the test

1. Mode **FT8**, band **20 m**, Monitor on, live audio from the radio.
2. Settings → Radio: PTT **RTS** or **DTR**, correct COM/`/dev/ttyUSB*` port.
3. Settings → Reporting:
   - UDP Server: `127.0.0.1`
   - Port: `2237`
   - **Accept UDP requests**: on
   - Outgoing interfaces: default is fine for localhost.
4. Apply / OK. Confirm a Heartbeat is leaving (the probe will print it).
5. Type 17 must show `Supported=true`. If it stays false, PTT is CAT/VOX or Accept UDP is off.

GridTracker or other apps bound to 2237 will steal unicast datagrams. For this test, point UDP Server at the probe only, or use a free port (example `2239`) in both WSJT-X and the probe.

---

## How to measure (do this)

### Software path (do first)

```text
python probe_tx_inhibit_latency.py --listen 127.0.0.1 --port 2237
```

Wait until it prints a Heartbeat and `Supported=true`.

Then:

```text
python probe_tx_inhibit_latency.py --listen 127.0.0.1 --port 2237 --auto --csv inhibit-latency.csv
```

`--auto` sends a 400 ms hold when Status says `Decoding=true` or `Transmitting=true`, and records:

- `t_send` → first type-17 `Inhibited=true` (`lat_status_us`)
- Status `Transmitting` going false if it was in TX (`lat_unkey_us`)

Collect at least:

- 20 holds in the **decode burst** (last ~2 s of the RX period, Band Activity filling)
- 20 holds in **mid-TX** (Enable Tx on, after PTT has been keyed for a second)
- 20 holds in a quiet moment (Monitor on, no decode, Enable Tx off)

Print min / p50 / p95 / max. Save the raw CSV the script writes.

Type 17 is GUI-thread telemetry. It is a lower bound on “command made it through the GUI.” It can be slower than the RTS pin (extra hop back to the GUI). It is still the right first measurement.

### Hardware path (do if a spare adapter exists)

PTT drop on the wire is the number that matters for W2SZ.

Do not open the same COM port WSJT-X is using. Use a tap:

- A second USB-serial whose CTS/DSR watches the PTT line, or
- WIMS keyline hardware if it is already on this desk, or
- A scope / logic probe on RTS.

Timestamp the falling edge against `t_send` from the probe (`t_send_ns` in the CSV). Same three windows: decode burst, mid-TX, idle.

If there is no tap, report type-17 latency and say the RTS edge was not captured.

### WAV fallback (only if 20 m is dead)

File → Open a stock FT8 sample (about 21 decodes; Deep = three Decode flashes). Send holds while Decode is lit. This is a one-shot burst, not a repeating 15 s load. Prefer live RF.

---

## Pass / fail against the 30 ms budget

The KEY-agent budget is ~30 ms from KEY sense to FT8 PTT release. Leave ~5–10 ms for the KEY agent and network. The WSJT-X hop should stay well under **20 ms p95**.

Report, for each window (decode / mid-TX / idle):

- n, min, p50, p95, max of `lat_status_us`
- n, min, p50, p95, max of `lat_unkey_us` and RTS edge if you have them
- Whether type 17 `Supported` stayed true
- Whether holds during TX actually unkeyed the radio (listen / wattmeter / RTS)

Interpretation:

- Idle ~1–3 ms and decode p95 also under ~5 ms: GUI-thread receive is fine on this PC for 20 m FT8.
- Decode p95 of tens of ms, idle still ~1 ms: the original concern is real under live load.
- Decode p95 over ~20 ms: 3.2’s GUI-thread receive can miss the W2SZ budget on this station.

---

## Pitfalls

- Type 18 goes to the **Heartbeat source port**. It does not go to 2237, and it does not go to PR #61’s dedicated inhibit port (22372 / type-17 advertised extra port).
- Id must match the Heartbeat Id exactly. Empty Id is dropped.
- TTL 99 is invalid and counted as `Invalid`; use 100–30000, or 0 to release.
- CAT or VOX PTT: type 18 is ignored for PTT. `Supported` stays false.
- A hold while already in RX does not move RTS (already idle). Use type 17 for that window; use TX for the pin.
- `tools/send_inhibit_hold.py` talks to this fork’s dedicated gate. Use `tools/probe_tx_inhibit_latency.py` against 3.2-rc1.

---

## What to write up when done

A short note with:

1. OS, 3.2-rc1 installer vs local build, PTT method and port.
2. The three latency tables (decode / mid-TX / idle).
3. Whether the radio unkeyed during TX holds.
4. One sentence on whether 3.2-rc1 on this PC stays inside the 30 ms budget under live 20 m decode.
5. The CSV path.

Jeff will use that to decide whether the GUI-thread receive path is acceptable for WIMS or still needs the dedicated-thread socket.

---

## Probe commands

```
python probe_tx_inhibit_latency.py --help
python probe_tx_inhibit_latency.py --listen 127.0.0.1 --port 2237
python probe_tx_inhibit_latency.py --listen 127.0.0.1 --port 2237 --auto --csv inhibit-latency.csv
```

`--auto` is the measurement. Interactive mode (no `--auto`): `h` hold 400 ms, `H` hold 2000 ms, `r` release, `q` quit.
