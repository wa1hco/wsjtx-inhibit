# WIMS TX Inhibit (in-tree)

Design authority:
[WIMS `docs/plan/wims_tx_inhibit.md`](https://github.com/wa1hco/WIMS/blob/main/docs/plan/wims_tx_inhibit.md)

## What changed in this fork

| Piece | Location |
|-------|----------|
| Pure gate logic (deadline, parse JSON datagram) | `TxInhibit/TxInhibitLogic.hpp` |
| Gate thread (UDP + serial RTS/DTR + CTS) | `TxInhibit/TxInhibitGate.{hpp,cpp}` |
| PTT routing (DTR/RTS → gate; hamlib VOX for PTT) | `Configuration.cpp` `open_rig` / `transceiver_ptt` |
| Status-bar badge | `widgets/mainwindow.cpp` `createStatusBar` |
| UDP `InhibitStatus` message type | `Network/NetworkMessage.hpp` + `MessageClient` |

## Behaviour (short)

- **WSJT-X TX decisions, audio, sequencer: unchanged.**
- Physical line: **`RTS or DTR = intent ∧ ¬inhibit`**.
- Inhibit sources OR-ed:
  1. UDP datagram on port **22372** (or ephemeral if busy):  
     `{"tx_inhibit":1,"ttl_ms":600,"station":"ROY-222-SSB","band":"222","seq":1}`  
     Release: `"ttl_ms":0`. Deadman expires hold if no keepalive.
  2. **CTS** on the same USB-serial PTT port (local KEY), with **WIMS adaptive hang**:
     short dit-like closures → hang ≈ 8×dit (clamped 0.2–1.0 s);
     long KEY (≥ ~0.75 s) or non-dit → hang ≈ **20 ms**.
     CTS must go idle once after open before KEY sense is live (avoids floating CTS).
     Disable KEY sense: env `WSJTX_INHIBIT_CTS=0`.
- Badge: `TX INHIBITED — …` while held.
- Plane A: `InhibitStatus` announces bind port + state (ignored by old clients).

## Operator setup

1. Settings → Radio: **PTT method = RTS** (or DTR) on a **dedicated** USB-serial
   port (Keyline Interface / similar). CAT on a different port.
2. Wire RTS → radio PTT (SEND). Optionally wire KEY sense → CTS on the same dongle.
3. Key agent (WIMS) unicasts hold/keepalive datagrams to the bound inhibit port
   (see Status / InhibitStatus).

CAT-PTT seats are **not** gated by this path (use WIMS external mute until rewired).

## Protocol alignment with WIMS Python spike

Matches `wims.interlock.inhibit` (`tx_inhibit` key, `ttl_ms` hold/release).
