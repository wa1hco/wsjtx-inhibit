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
import array
import csv
import fcntl
import os
import select
import socket
import struct
import sys
import termios
import time
from collections import defaultdict

TIOCMGET = 0x5415
TIOCM_RTS = 0x004

MAGIC = 0xADBCCBDA
TYPE_HEARTBEAT = 0
TYPE_STATUS = 1
TYPE_DECODE = 2
TYPE_REPLY = 4
TYPE_REPLAY = 7
TYPE_HALT_TX = 8
TYPE_FREE_TEXT = 9
TYPE_INHIBIT_STATUS = 17
TYPE_TX_INHIBIT = 18

CONTROLLER = "LATPROBE"
STATION = "LATPROBE"
FT8_PERIOD_S = 15.0
CSV_FIELDS = [
    "t_send_ns",
    "t_unix",
    "phase_ms",
    "window",
    "lat_status_us",
    "lat_pin_us",
    "lat_e2e_us",
    "lat_rts_us",
    "lat_unkey_us",
    "was_tx",
    "rts_before",
    "decoding",
    "transmitting",
]


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


def encode_header(schema: int, typ: int, ident: str) -> bytes:
    body = struct.pack(">III", MAGIC, schema, typ)
    body += qba(ident.encode("utf-8"))
    return body


def encode_tx_inhibit(schema: int, ident: str, ttl_ms: int) -> bytes:
    body = encode_header(schema, TYPE_TX_INHIBIT, ident)
    body += qba(CONTROLLER.encode("utf-8"))
    body += struct.pack(">I", int(ttl_ms) & 0xFFFFFFFF)
    body += qba(STATION.encode("utf-8"))
    return body


def encode_replay(schema: int, ident: str) -> bytes:
    return encode_header(schema, TYPE_REPLAY, ident)


def encode_halt_tx(schema: int, ident: str, auto_only: bool = True) -> bytes:
    # auto_only unchecks Enable Tx. The other value clicks Stop.
    return encode_header(schema, TYPE_HALT_TX, ident) + bytes([1 if auto_only else 0])


def encode_free_text(schema: int, ident: str, text: str, send: bool) -> bytes:
    # send=false and a non-empty string only writes the Tx5 box. The handler
    # still calls tx_watchdog(false), which is what clears the idle-minute count.
    body = encode_header(schema, TYPE_FREE_TEXT, ident)
    body += qba(text.encode("utf-8"))
    body += bytes([1 if send else 0])
    return body


def encode_reply(schema: int, ident: str, dec: dict) -> bytes:
    body = encode_header(schema, TYPE_REPLY, ident)
    body += struct.pack(">Ii", int(dec["time_ms"]) & 0xFFFFFFFF, int(dec["snr"]))
    body += struct.pack(">d", float(dec["dt"]))
    body += struct.pack(">I", int(dec["df"]) & 0xFFFFFFFF)
    body += qba(dec["mode"].encode("utf-8"))
    body += qba(dec["message"].encode("utf-8"))
    body += bytes([1 if dec["low_confidence"] else 0, 0])
    return body


def parse_inhibit_status_32(buf: bytes, off: int):
    supported, off = read_bool(buf, off)
    inhibited, off = read_bool(buf, off)
    holder, off = read_qba(buf, off)
    hold_rx, off = read_u32(buf, off)
    release_rx, off = read_u32(buf, off)
    expiries, off = read_u32(buf, off)
    invalid, off = read_u32(buf, off)
    t_rx_ns = t_pin_ns = 0
    if off + 16 <= len(buf):
        t_rx_ns, t_pin_ns = struct.unpack_from(">QQ", buf, off)
    return {
        "supported": supported,
        "inhibited": inhibited,
        "holder": holder.decode("utf-8", "replace"),
        "port": 0,
        "hold_rx": hold_rx,
        "release_rx": release_rx,
        "expiries": expiries,
        "invalid": invalid,
        "t_rx_ns": int(t_rx_ns),
        "t_pin_ns": int(t_pin_ns),
    }


def parse_inhibit_status_fork(buf: bytes, off: int):
    # Id already consumed. Then: quint16 port, bool inhibited, utf8 station, 4×quint32
    if off + 2 > len(buf):
        raise ValueError("truncated inhibit port")
    (port,) = struct.unpack_from(">H", buf, off)
    off += 2
    inhibited, off = read_bool(buf, off)
    holder, off = read_qba(buf, off)
    hold_rx, off = read_u32(buf, off)
    release_rx, off = read_u32(buf, off)
    expiries, off = read_u32(buf, off)
    invalid, off = read_u32(buf, off)
    return {
        "supported": port != 0,
        "inhibited": inhibited,
        "holder": holder.decode("utf-8", "replace"),
        "port": int(port),
        "hold_rx": hold_rx,
        "release_rx": release_rx,
        "expiries": expiries,
        "invalid": invalid,
    }


def parse_decode(buf: bytes, off: int):
    new, off = read_bool(buf, off)
    time_ms, off = read_u32(buf, off)
    if off + 4 > len(buf):
        raise ValueError("truncated snr")
    (snr,) = struct.unpack_from(">i", buf, off)
    off += 4
    if off + 8 > len(buf):
        raise ValueError("truncated dt")
    (dt,) = struct.unpack_from(">d", buf, off)
    off += 8
    df, off = read_u32(buf, off)
    mode, off = read_qba(buf, off)
    message, off = read_qba(buf, off)
    low, off = read_bool(buf, off)
    off_air = False
    if off < len(buf):
        off_air, off = read_bool(buf, off)
    return {
        "new": new,
        "time_ms": time_ms,
        "snr": snr,
        "dt": dt,
        "df": df,
        "mode": mode.decode("utf-8", "replace"),
        "message": message.decode("utf-8", "replace"),
        "low_confidence": low,
        "off_air": off_air,
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
        f"  {label:28s} n={len(values_us):4d}  "
        f"min={min(values_us):8.0f}  p50={pct(values_us, 0.50):8.0f}  "
        f"p95={pct(values_us, 0.95):8.0f}  max={max(values_us):8.0f} us"
    )


def ft8_phase_ms(t_unix: float) -> int:
    return int((t_unix % FT8_PERIOD_S) * 1000.0)


class Probe:
    def __init__(self, listen: str, port: int, csv_path: str | None, quiet: bool = False,
                 ptt_port: str | None = None, fork: bool = False, mcast: str | None = None):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if mcast:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1) if hasattr(socket, "SO_REUSEPORT") else None
            self.sock.bind(("", port) if listen in ("127.0.0.1", "0.0.0.0") else (listen, port))
            mreq = struct.pack("=4s4s", socket.inet_aton(mcast), socket.inet_aton("0.0.0.0"))
            self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            print(f"joined multicast {mcast}:{port}")
        else:
            self.sock.bind((listen, port))
        self.sock.setblocking(False)
        self.fork = fork
        self.endpoint = None  # where type-18 is sent
        self.hb_addr = None
        self.ident = None
        self.schema = 3
        self.supported = False
        self.inhibited = False
        self.transmitting = False
        self.decoding = False
        self.tx_enabled = False
        self.mode = ""
        self.decodes = []
        self.replied = False
        self.pending_send_ns = None
        self.pending_mono_ns = None
        self.pending_unix = None
        self.pending_window = None
        self.pending_was_tx = False
        self.pending_decoding = False
        self.pending_rts_before = False
        self.pending_rts_us = ""
        self.rows = []
        self.by_window = defaultdict(lambda: {"status": [], "pin": [], "e2e": [], "unkey": [], "rts": []})
        self.csv_path = csv_path
        self.last_auto_ns = 0
        self.quiet = quiet
        self._hb_printed = False
        self.ptt_fd = None
        if ptt_port:
            self.ptt_fd = os.open(ptt_port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            print(f"PTT tap {ptt_port} (TIOCMGET RTS, no line changes)")
        print(f"listening as UDP Server on {listen}:{port}")

    def rts(self):
        if self.ptt_fd is None:
            return None
        buf = array.array("I", [0])
        try:
            fcntl.ioctl(self.ptt_fd, TIOCMGET, buf)
        except OSError:
            return None
        return bool(buf[0] & TIOCM_RTS)

    def log(self, msg: str):
        if not self.quiet:
            print(msg)

    def _send(self, payload: bytes) -> bool:
        if not self.endpoint or not self.ident:
            return False
        self.sock.sendto(payload, self.endpoint)
        return True

    def best_decode(self, skip: set | None = None):
        """Newest on-air decode Reply can still find in Band Activity.

        Skip our own call so a contest-shaped exchange cannot pop a modal
        settings dialog. Prefer a CQ; Hold Tx Freq accepts any other line.
        """
        skip = skip or set()
        best = None
        best_score = 0
        now = time.monotonic()
        for dec in reversed(self.decodes):
            if now - dec.get("t_mono", now) > 180:
                continue
            key = (dec["time_ms"], dec["df"], dec["message"])
            if key in skip:
                continue
            msg = dec["message"].strip()
            if dec["off_air"] or "WA1HCO" in msg.upper() or len(msg) < 4:
                continue
            score = 3 if msg.startswith(("CQ ", "CQDX ", "QRZ ")) else 1
            if score > best_score:
                best = dec
                best_score = score
                if score >= 3:
                    break
        return best

    def send_reply(self, dec: dict) -> None:
        if not self._send(encode_reply(self.schema, self.ident, dec)):
            return
        self.replied = True
        print(
            f"sent Reply to {dec['message']!r} df={dec['df']} snr={dec['snr']} "
            f"-> {self.endpoint[0]}:{self.endpoint[1]}"
        )

    def keepalive(self) -> None:
        # Does not click a Tx button. Resets the 6-minute idle watchdog.
        if self._send(encode_free_text(self.schema, self.ident, "TNX 73", False)):
            print("sent Free Text keepalive (watchdog reset, selected Tx message unchanged)")

    def halt_enable_tx(self) -> None:
        if self._send(encode_halt_tx(self.schema, self.ident, True)):
            print("sent Halt Tx (Auto Tx only) so Enable Tx is off")
            self.pump(0.2)

    def wait_transmitting(self, wait_s: float) -> bool:
        deadline = time.time() + wait_s
        while time.time() < deadline:
            self.pump(0.05)
            if self.transmitting:
                print(
                    f"Status Transmitting=yes mode={self.mode!r} "
                    f"tx_enabled={int(self.tx_enabled)}"
                )
                return True
        print(
            f"Transmitting did not come on within {wait_s:.0f}s "
            f"(tx_enabled={int(self.tx_enabled)} mode={self.mode!r})"
        )
        return False

    def arm_tx(self, wait_s: float = 180.0) -> bool:
        """Check Enable Tx with Reply (type 4) unless it is already on.

        Reply has to match a line still in Band Activity. Replay asks WS to
        resend those lines. Quick Call and Hold Tx Freq are on for this station,
        so a matched Reply checks Enable Tx and tx_watchdog(false) runs too.
        10 m can be quiet for a minute, so Replay is repeated until a line shows up.
        """
        deadline = time.time() + wait_s
        last_replay = 0.0
        tried: set = set()
        last_reply = 0.0
        announced = 0
        while time.time() < deadline:
            self.pump(0.05)
            if not self.endpoint or not self.ident:
                continue
            if self.tx_enabled and not self.replied:
                print(f"Enable Tx already on, mode={self.mode!r}. Not sending Reply.")
                return self.wait_transmitting(25.0)
            if len(self.decodes) != announced:
                latest = self.decodes[-1]
                print(
                    f"decode {latest['message']!r} df={latest['df']} "
                    f"snr={latest['snr']} (have {len(self.decodes)})"
                )
                announced = len(self.decodes)
            if self.replied and self.tx_enabled:
                print("Enable Tx is on after Reply")
                return self.wait_transmitting(25.0)
            now = time.time()
            if now - last_replay >= 10.0:
                if self._send(encode_replay(self.schema, self.ident)):
                    print("sent Replay for the lines still in Band Activity")
                last_replay = now
            if now - last_reply < 1.5:
                continue
            dec = self.best_decode(tried)
            if not dec:
                continue
            tried.add((dec["time_ms"], dec["df"], dec["message"]))
            self.send_reply(dec)
            last_reply = now
        print("arm failed: Enable Tx did not come on (no matching decode, or Reply was ignored)")
        return False

    def rearm_once(self) -> None:
        dec = self.best_decode()
        if dec:
            self.send_reply(dec)
        elif self._send(encode_replay(self.schema, self.ident)):
            print("Enable Tx dropped; sent Replay to find a line to answer")

    def send_hold(self, ttl_ms: int, window: str):
        if not self.endpoint or not self.ident:
            print("no Heartbeat yet")
            return
        payload = encode_tx_inhibit(self.schema, self.ident, ttl_ms)
        t_unix = time.time()
        rts_before = self.rts()
        t0 = time.perf_counter_ns()
        t_mono = time.monotonic_ns()
        dest = self.endpoint
        self.sock.sendto(payload, dest)
        self.pending_send_ns = t0
        self.pending_mono_ns = t_mono
        self.pending_unix = t_unix
        self.pending_window = window
        self.pending_was_tx = bool(self.transmitting or rts_before)
        self.pending_decoding = self.decoding
        self.pending_rts_before = bool(rts_before)
        self.pending_rts_us = ""
        self.log(
            f"sent type-18 ttl={ttl_ms} -> {self.endpoint[0]}:{self.endpoint[1]} "
            f"id={self.ident!r} window={window} phase={ft8_phase_ms(t_unix)} "
            f"tx={int(self.transmitting)} rts={rts_before} dec={int(self.decoding)}"
        )

    def poll_after_send(self, timeout_ns=50_000_000):
        """Pump UDP and watch RTS fall. Call immediately after send_hold."""
        t0 = self.pending_send_ns
        if t0 is None:
            return
        deadline = t0 + timeout_ns
        while time.perf_counter_ns() < deadline:
            self.pump(0.0)
            if (self.pending_rts_before and not self.pending_rts_us
                    and self.rts() is False):
                self.pending_rts_us = f"{(time.perf_counter_ns() - t0) / 1000.0:.1f}"
                if self.rows:
                    self.rows[-1]["lat_rts_us"] = self.pending_rts_us
            if self.pending_send_ns is None and (
                    not self.pending_rts_before or self.pending_rts_us):
                break
        if self.pending_rts_us:
            self.by_window[self.pending_window or "tx"]["rts"].append(
                float(self.pending_rts_us)
            )

    def note_status_true(self, t_rx_ns=0, t_pin_ns=0):
        if self.pending_send_ns is None:
            return
        dt = (time.perf_counter_ns() - self.pending_send_ns) / 1000.0
        pin = e2e = ""
        if t_rx_ns and t_pin_ns and t_pin_ns > t_rx_ns:
            pin_us = (t_pin_ns - t_rx_ns) / 1000.0
            pin = f"{pin_us:.1f}"
            self.by_window[self.pending_window]["pin"].append(pin_us)
        if t_pin_ns and self.pending_mono_ns and t_pin_ns > self.pending_mono_ns:
            e2e_us = (t_pin_ns - self.pending_mono_ns) / 1000.0
            e2e = f"{e2e_us:.1f}"
            self.by_window[self.pending_window]["e2e"].append(e2e_us)
        self.by_window[self.pending_window]["status"].append(dt)
        self.rows.append(
            {
                "t_send_ns": self.pending_send_ns,
                "t_unix": f"{self.pending_unix:.6f}",
                "phase_ms": ft8_phase_ms(self.pending_unix),
                "window": self.pending_window,
                "lat_status_us": f"{dt:.1f}",
                "lat_pin_us": pin,
                "lat_e2e_us": e2e,
                "lat_rts_us": self.pending_rts_us,
                "lat_unkey_us": "",
                "was_tx": int(self.pending_was_tx),
                "rts_before": int(self.pending_rts_before),
                "decoding": int(self.pending_decoding),
                "transmitting": int(self.transmitting),
            }
        )
        extra = f"  pin={pin}us  send→pin={e2e}us" if pin else ""
        self.log(
            f"  type-17 Inhibited=true  arrive={dt:.0f} us{extra}  "
            f"window={self.pending_window} phase={ft8_phase_ms(self.pending_unix)}"
        )
        self.pending_send_ns = None

    def note_unkey(self):
        if not self.rows or not self.pending_was_tx:
            return
        if self.pending_unix is None:
            return
        # unkey timing is from last send that was in TX
        last = self.rows[-1]
        if last.get("was_tx") in (1, "1") and not last.get("lat_unkey_us"):
            t0 = int(last["t_send_ns"])
            dt = (time.perf_counter_ns() - t0) / 1000.0
            last["lat_unkey_us"] = f"{dt:.1f}"
            self.by_window[last["window"]]["unkey"].append(dt)
            self.log(f"  Status Transmitting=false {dt:.0f} us  window={last['window']}")
        self.pending_was_tx = False

    def handle(self, data: bytes, addr):
        try:
            schema, typ, ident, off = parse_header(data)
        except ValueError:
            return
        self.schema = schema if schema in (2, 3) else self.schema
        if typ == TYPE_HEARTBEAT:
            self.hb_addr = addr
            self.ident = ident
            self.endpoint = addr
            if not self._hb_printed or not self.quiet or not self.supported:
                print(f"Heartbeat id={ident!r} schema={schema} from {addr[0]}:{addr[1]}"
                      f"{'' if self.supported else ' (waiting for type-17 Supported)'}")
                self._hb_printed = True
            return
        if typ == TYPE_INHIBIT_STATUS:
            try:
                st = parse_inhibit_status_32(data, off)
            except ValueError as e:
                print(f"type-17 parse fail ({e}) n={len(data)} off={off} hex={data[off:off+24].hex()}")
                return
            prev = self.inhibited
            self.supported = st["supported"]
            self.inhibited = st["inhibited"]
            self.log(
                f"InhibitStatus supported={int(st['supported'])} "
                f"inhibited={int(st['inhibited'])} holder={st['holder']!r} "
                f"port={st.get('port', 0)} hold_rx={st['hold_rx']} invalid={st['invalid']}"
            )
            if st["inhibited"] and not prev:
                self.note_status_true(st.get("t_rx_ns", 0), st.get("t_pin_ns", 0))
            return
        if typ == TYPE_STATUS:
            try:
                st = parse_status(data, off)
            except ValueError:
                return
            was_tx = self.transmitting
            self.tx_enabled = st["tx_enabled"]
            self.mode = st["mode"]
            self.transmitting = st["transmitting"]
            self.decoding = st["decoding"]
            if was_tx and not st["transmitting"]:
                self.note_unkey()
            return
        if typ == TYPE_DECODE:
            try:
                dec = parse_decode(data, off)
            except ValueError:
                return
            dec["t_mono"] = time.monotonic()
            self.decodes.append(dec)
            if len(self.decodes) > 120:
                self.decodes = self.decodes[-120:]
            return

    def pump(self, timeout=0.02):
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
            w = csv.DictWriter(f, fieldnames=CSV_FIELDS)
            w.writeheader()
            w.writerows(self.rows)

    def report(self):
        print("\nLatency: socket→RTS ioctl, send→RTS ioctl, type-17 arrival")
        for window in ("decode", "tx", "idle", "manual"):
            summarize(f"{window} socket→RTS", self.by_window[window]["pin"])
            summarize(f"{window} send→RTS", self.by_window[window]["e2e"])
            summarize(f"{window} type-17", self.by_window[window]["status"])
            summarize(f"{window} RTS fall", self.by_window[window]["rts"])
            summarize(f"{window} unkey", self.by_window[window]["unkey"])
        self.write_csv()
        if self.csv_path and self.rows:
            print(f"wrote {self.csv_path}  n={len(self.rows)}")


def classify(p: Probe) -> str:
    rts = p.rts()
    if p.transmitting or rts:
        return "tx"
    if p.decoding:
        return "decode"
    return "idle"


def run_auto(p: Probe, n_each: int, ttl: int, interval_ms: int, wait_clear: bool):
    want = {"decode": n_each, "tx": n_each, "idle": n_each}
    got = {"decode": 0, "tx": 0, "idle": 0}
    interval_ns = interval_ms * 1_000_000
    print(f"auto: {n_each} holds each in decode / tx / idle, ttl={ttl} ms, "
          f"interval={interval_ms} ms")
    print("Enable Tx on the radio for the tx window. Ctrl-C when done.")
    try:
        while any(got[k] < want[k] for k in want):
            p.pump(0.02)
            now = time.perf_counter_ns()
            if p.pending_send_ns is not None:
                if now - p.pending_send_ns > 800_000_000:
                    print("  timeout waiting for type-17")
                    p.pending_send_ns = None
                continue
            if wait_clear and p.inhibited:
                continue
            if not p.endpoint or not p.supported:
                continue
            if now - p.last_auto_ns < interval_ns:
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


def run_sweep(p: Probe, duration_s: float, ttl: int, interval_ms: int,
              arm: bool = False, keepalive_s: float = 90.0) -> int:
    import signal

    def _stop(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, _stop)
    if arm:
        print("arming Enable Tx with Reply before the sweep")
        if not p.arm_tx():
            if p.replied or p.tx_enabled:
                p.halt_enable_tx()
            print("not starting the sweep")
            return 2
    interval_ns = interval_ms * 1_000_000
    t_end = time.time() + duration_s
    n = 0
    print(f"sweep: {duration_s:.0f}s  ttl={ttl} ms  interval={interval_ms} ms  "
          f"(~{int(1000 / interval_ms)} holds/s, {int(15000 / interval_ms)} per 15s period)")
    if arm:
        print(f"Free Text keepalive every {keepalive_s:.0f}s resets the {6}-minute TX watchdog. "
              "Halt Tx clears Enable Tx when the sweep ends.")
    else:
        print("Samples the whole FT8 cycle. Enable Tx to include mid-TX bins. Ctrl-C to stop.")
    last_flush = 0
    last_ka = time.time()
    try:
        while time.time() < t_end:
            p.pump(0.01)
            now = time.perf_counter_ns()
            if p.pending_send_ns is not None:
                if now - p.pending_send_ns > 500_000_000:
                    p.pending_send_ns = None
                continue
            if arm and p.endpoint and (time.time() - last_ka) >= keepalive_s:
                if p.tx_enabled:
                    p.keepalive()
                else:
                    print("Enable Tx is off; sending another Reply")
                    p.rearm_once()
                last_ka = time.time()
            if p.inhibited:
                continue
            if not p.endpoint or not p.supported:
                continue
            if now - p.last_auto_ns < interval_ns:
                continue
            p.send_hold(ttl, classify(p))
            p.poll_after_send()
            n += 1
            p.last_auto_ns = now
            if n % 25 == 0 or n == 1:
                left = t_end - time.time()
                last = p.rows[-1]["lat_status_us"] if p.rows else "-"
                counts = {w: len(p.by_window[w]["status"]) for w in ("idle", "decode", "tx")}
                print(f"  n={n:4d}  last={last}us  idle={counts['idle']} "
                      f"decode={counts['decode']} tx={counts['tx']}  "
                      f"tx_en={int(p.tx_enabled)}  left={left:.0f}s")
            if n - last_flush >= 25:
                p.write_csv()
                last_flush = n
    except KeyboardInterrupt:
        print("stopped")
    finally:
        if arm:
            p.halt_enable_tx()
    p.report()
    return 0


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
    ap.add_argument("--interval-ms", type=int, default=1500,
                    help="minimum gap between auto/sweep holds")
    ap.add_argument("--arm-tx", action="store_true",
                    help="Reply to a decode to check Enable Tx, reset the TX watchdog, "
                         "and Halt Tx when the sweep ends")
    ap.add_argument("--keepalive-s", type=float, default=90.0,
                    help="with --arm-tx, seconds between Free Text watchdog resets")
    ap.add_argument("--sweep-s", type=float, default=0,
                    help="dense sweep for this many seconds (all windows, no per-window cap)")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--fork", action="store_true",
                    help="wsjtx-inhibit type-17 (ephemeral port) instead of 3.2-rc1")
    ap.add_argument("--mcast", default="",
                    help="join this multicast group (fork UDP Server, e.g. 224.0.0.73)")
    ap.add_argument("--ptt-port", default="",
                    help="serial device to TIOCMGET RTS (same PTT port WSJT-X uses)")
    ap.add_argument("--csv", default="inhibit-latency.csv")
    args = ap.parse_args()
    if args.ttl_ms and not (args.ttl_ms == 0 or 100 <= args.ttl_ms <= 30000):
        sys.exit("ttl-ms must be 0 or 100..30000")
    if args.interval_ms < 50:
        sys.exit("interval-ms must be >= 50")
    p = Probe(args.listen, args.port, args.csv, quiet=args.quiet or args.sweep_s > 0,
              ptt_port=(args.ptt_port or None), fork=args.fork,
              mcast=(args.mcast or None))
    if args.sweep_s > 0:
        rc = run_sweep(p, args.sweep_s, args.ttl_ms, args.interval_ms,
                       arm=args.arm_tx, keepalive_s=args.keepalive_s)
        sys.exit(rc)
    elif args.auto:
        run_auto(p, args.n, args.ttl_ms, args.interval_ms, wait_clear=True)
    else:
        run_interactive(p, args.ttl_ms)


if __name__ == "__main__":
    main()
