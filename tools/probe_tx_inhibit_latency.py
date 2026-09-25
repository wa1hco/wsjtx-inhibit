#!/usr/bin/env python3
"""Discover a WSJT-X 3.2-rc1 instance and measure type-18 TX Inhibit latency.

Bind as the UDP Server (default 127.0.0.1:2237), learn the Heartbeat source
endpoint and Id, send NetworkMessage::TxInhibit (type 18) back to that
endpoint, and time type-17 InhibitStatus Inhibited=true (and Status
Transmitting falling, when a hold lands in a TX period).

Stdlib only. See HANDOFF-3.2-rc1-inhibit-latency.md.
"""
from __future__ import annotations

import argparse
import csv
import select
import socket
import struct
import sys
import time
from collections import defaultdict

MAGIC = 0xADBCCBDA
TYPE_HEARTBEAT = 0
TYPE_STATUS = 1
TYPE_DECODE = 2
TYPE_INHIBIT_STATUS = 17
TYPE_TX_INHIBIT = 18

CONTROLLER = "LATPROBE"
STATION = "LATPROBE"


def qba(data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + data


def read_qba(buf: bytes, off: int):
    if off + 4 > len(buf):
        raise ValueError("truncated QByteArray length")
    (n,) = struct.unpack_from(">I", buf, off)
    off += 4
    if n == 0xFFFFFFFF:
        return b"", off
    if n > 4096 or off + n > len(buf):
        raise ValueError("truncated QByteArray body")
    return buf[off : off + n], off + n


def read_bool(buf: bytes, off: int):
    if off >= len(buf):
        raise ValueError("truncated bool")
    return buf[off] != 0, off + 1


def read_u32(buf: bytes, off: int):
    if off + 4 > len(buf):
        raise ValueError("truncated u32")
    (v,) = struct.unpack_from(">I", buf, off)
    return v, off + 4


def parse_header(buf: bytes):
    if len(buf) < 16:
        raise ValueError("short datagram")
    magic, schema, typ = struct.unpack_from(">III", buf, 0)
    if magic != MAGIC:
        raise ValueError("bad magic")
    ident, off = read_qba(buf, 12)
    return schema, typ, ident.decode("utf-8", "replace"), off


def encode_tx_inhibit(schema: int, ident: str, ttl_ms: int) -> bytes:
    body = struct.pack(">III", MAGIC, schema, TYPE_TX_INHIBIT)
    body += qba(ident.encode("utf-8"))
    body += qba(CONTROLLER.encode("utf-8"))
    body += struct.pack(">I", int(ttl_ms) & 0xFFFFFFFF)
    body += qba(STATION.encode("utf-8"))
    return body


def parse_inhibit_status(buf: bytes, off: int):
    supported, off = read_bool(buf, off)
    inhibited, off = read_bool(buf, off)
    holder, off = read_qba(buf, off)
    hold_rx, off = read_u32(buf, off)
    release_rx, off = read_u32(buf, off)
    expiries, off = read_u32(buf, off)
    invalid, off = read_u32(buf, off)
    return {
        "supported": supported,
        "inhibited": inhibited,
        "holder": holder.decode("utf-8", "replace"),
        "hold_rx": hold_rx,
        "release_rx": release_rx,
        "expiries": expiries,
        "invalid": invalid,
    }


def parse_status(buf: bytes, off: int):
    # Dial Frequency quint64, then several utf8, then three bools:
    # Tx Enabled, Transmitting, Decoding
    if off + 8 > len(buf):
        raise ValueError("truncated status freq")
    (freq,) = struct.unpack_from(">Q", buf, off)
    off += 8
    _mode, off = read_qba(buf, off)
    _dx, off = read_qba(buf, off)
    _rpt, off = read_qba(buf, off)
    _txmode, off = read_qba(buf, off)
    tx_enabled, off = read_bool(buf, off)
    transmitting, off = read_bool(buf, off)
    decoding, off = read_bool(buf, off)
    return {
        "freq": freq,
        "tx_enabled": tx_enabled,
        "transmitting": transmitting,
        "decoding": decoding,
        "mode": _mode.decode("utf-8", "replace"),
    }


def pct(values, p):
    if not values:
        return 0.0
    s = sorted(values)
    idx = int(round((len(s) - 1) * p))
    return s[min(idx, len(s) - 1)]


def summarize(label, values_us):
    if not values_us:
        print(f"  {label:28s} n=0")
        return
    print(
        f"  {label:28s} n={len(values_us):2d}  "
        f"min={min(values_us):8.0f}  p50={pct(values_us, 0.50):8.0f}  "
        f"p95={pct(values_us, 0.95):8.0f}  max={max(values_us):8.0f} us"
    )


class Probe:
    def __init__(self, listen: str, port: int, csv_path: str | None):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((listen, port))
        self.sock.setblocking(False)
        self.endpoint = None
        self.ident = None
        self.schema = 3
        self.supported = False
        self.inhibited = False
        self.transmitting = False
        self.decoding = False
        self.pending_send_ns = None
        self.pending_window = None
        self.pending_was_tx = False
        self.rows = []
        self.by_window = defaultdict(lambda: {"status": [], "unkey": []})
        self.csv_path = csv_path
        self.last_auto_ns = 0
        print(f"listening as UDP Server on {listen}:{port}")

    def send_hold(self, ttl_ms: int, window: str):
        if not self.endpoint or not self.ident:
            print("no Heartbeat yet")
            return
        payload = encode_tx_inhibit(self.schema, self.ident, ttl_ms)
        t0 = time.perf_counter_ns()
        self.sock.sendto(payload, self.endpoint)
        self.pending_send_ns = t0
        self.pending_window = window
        self.pending_was_tx = self.transmitting
        print(
            f"sent type-18 ttl={ttl_ms} -> {self.endpoint[0]}:{self.endpoint[1]} "
            f"id={self.ident!r} window={window} tx={int(self.transmitting)} "
            f"dec={int(self.decoding)}"
        )

    def note_status_true(self):
        if self.pending_send_ns is None:
            return
        dt = (time.perf_counter_ns() - self.pending_send_ns) / 1000.0
        self.by_window[self.pending_window]["status"].append(dt)
        self.rows.append(
            {
                "t_send_ns": self.pending_send_ns,
                "window": self.pending_window,
                "lat_status_us": f"{dt:.1f}",
                "lat_unkey_us": "",
                "was_tx": int(self.pending_was_tx),
            }
        )
        print(f"  type-17 Inhibited=true  {dt:.0f} us  window={self.pending_window}")

    def note_unkey(self):
        if self.pending_send_ns is None or not self.pending_was_tx:
            return
        dt = (time.perf_counter_ns() - self.pending_send_ns) / 1000.0
        self.by_window[self.pending_window]["unkey"].append(dt)
        if self.rows:
            self.rows[-1]["lat_unkey_us"] = f"{dt:.1f}"
        print(f"  Status Transmitting=false {dt:.0f} us  window={self.pending_window}")
        self.pending_was_tx = False

    def handle(self, data: bytes, addr):
        try:
            schema, typ, ident, off = parse_header(data)
        except ValueError:
            return
        self.schema = schema if schema in (2, 3) else self.schema
        if typ == TYPE_HEARTBEAT:
            self.endpoint = addr
            self.ident = ident
            print(f"Heartbeat id={ident!r} schema={schema} from {addr[0]}:{addr[1]}")
            return
        if typ == TYPE_INHIBIT_STATUS:
            try:
                st = parse_inhibit_status(data, off)
            except ValueError:
                return
            prev = self.inhibited
            self.supported = st["supported"]
            self.inhibited = st["inhibited"]
            print(
                f"InhibitStatus supported={int(st['supported'])} "
                f"inhibited={int(st['inhibited'])} holder={st['holder']!r} "
                f"hold_rx={st['hold_rx']} invalid={st['invalid']}"
            )
            if st["inhibited"] and not prev:
                self.note_status_true()
            return
        if typ == TYPE_STATUS:
            try:
                st = parse_status(data, off)
            except ValueError:
                return
            was_tx = self.transmitting
            self.transmitting = st["transmitting"]
            self.decoding = st["decoding"]
            if was_tx and not st["transmitting"]:
                self.note_unkey()
            return
        if typ == TYPE_DECODE:
            return

    def pump(self, timeout=0.05):
        r, _, _ = select.select([self.sock], [], [], timeout)
        if not r:
            return
        try:
            data, addr = self.sock.recvfrom(4096)
        except BlockingIOError:
            return
        self.handle(data, addr)

    def write_csv(self):
        if not self.csv_path or not self.rows:
            return
        with open(self.csv_path, "w", newline="") as f:
            w = csv.DictWriter(
                f,
                fieldnames=[
                    "t_send_ns",
                    "window",
                    "lat_status_us",
                    "lat_unkey_us",
                    "was_tx",
                ],
            )
            w.writeheader()
            w.writerows(self.rows)
        print(f"wrote {self.csv_path}")

    def report(self):
        print("\nLatency hold send → type-17 Inhibited / Status unkey")
        for window in ("decode", "tx", "idle", "manual"):
            summarize(f"{window} type-17", self.by_window[window]["status"])
            summarize(f"{window} unkey", self.by_window[window]["unkey"])
        self.write_csv()


def classify(p: Probe) -> str:
    if p.decoding:
        return "decode"
    if p.transmitting:
        return "tx"
    return "idle"


def run_auto(p: Probe, n_each: int, ttl: int):
    want = {"decode": n_each, "tx": n_each, "idle": n_each}
    got = {"decode": 0, "tx": 0, "idle": 0}
    print(f"auto: {n_each} holds each in decode / tx / idle, ttl={ttl} ms")
    print("Enable Tx on the radio for the tx window. Ctrl-C when done.")
    try:
        while any(got[k] < want[k] for k in want):
            p.pump(0.05)
            now = time.perf_counter_ns()
            if p.pending_send_ns is not None:
                if now - p.pending_send_ns > 800_000_000:
                    print("  timeout waiting for type-17")
                    p.pending_send_ns = None
                continue
            if not p.endpoint or not p.supported:
                continue
            if now - p.last_auto_ns < 1_500_000_000:
                continue
            window = classify(p)
            if got[window] >= want[window]:
                continue
            p.send_hold(ttl, window)
            got[window] += 1
            p.last_auto_ns = now
            print(f"  progress decode={got['decode']}/{n_each} "
                  f"tx={got['tx']}/{n_each} idle={got['idle']}/{n_each}")
    except KeyboardInterrupt:
        print("stopped")
    p.report()


def _read_key():
    if sys.platform == "win32":
        import msvcrt
        if msvcrt.kbhit():
            ch = msvcrt.getwch()
            return ch
        return None
    r, _, _ = select.select([sys.stdin], [], [], 0)
    if r:
        return sys.stdin.read(1)
    return None


def run_interactive(p: Probe, ttl: int):
    print("keys: h=hold 400ms  H=hold 2000ms  r=release  q=quit")
    try:
        while True:
            p.pump(0.05)
            ch = _read_key()
            if not ch:
                continue
            if ch in ("q", "Q"):
                break
            if ch == "h":
                p.send_hold(ttl, "manual")
            elif ch == "H":
                p.send_hold(2000, "manual")
            elif ch in ("r", "R"):
                p.send_hold(0, "manual")
    except KeyboardInterrupt:
        print("stopped")
    p.report()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--listen", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=2237)
    ap.add_argument("--ttl-ms", type=int, default=400)
    ap.add_argument("--auto", action="store_true")
    ap.add_argument("--n", type=int, default=20, help="holds per window in --auto")
    ap.add_argument("--csv", default="inhibit-latency.csv")
    args = ap.parse_args()
    if args.ttl_ms and not (args.ttl_ms == 0 or 100 <= args.ttl_ms <= 30000):
        sys.exit("ttl-ms must be 0 or 100..30000")
    p = Probe(args.listen, args.port, args.csv)
    if args.auto:
        run_auto(p, args.n, args.ttl_ms)
    else:
        run_interactive(p, args.ttl_ms)


if __name__ == "__main__":
    main()
