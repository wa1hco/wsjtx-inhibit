#!/usr/bin/env python3
"""KEY-agent stand-in: send TX Inhibit hold/release UDP datagrams to a gate.

Protocol and KEY-agent design: docs/TX_INHIBIT.md

Grave/backtick ` is KEY *level* (not Space). Windows: VK_OEM_3; Linux: KEY_GRAVE
(/dev/input; may need group input).

  python3 tools/send_inhibit_hold.py --interactive
  python3 tools/send_inhibit_hold.py --ttl-ms 3000 --station TEST
  python3 tools/send_inhibit_hold.py --ttl-ms 0

Binds the UDP server WS sends Heartbeat and type 17 to (default 127.0.0.1:2237).
Type 18 is sent back to the heartbeat source, with that message's Id and schema.
"""
from __future__ import annotations

import argparse
import os
import select
import socket
import struct
import sys
import time

DEFAULT_TTL_MS = 600  # hold_timeout_ms (safety), not hang
KEEPALIVE_S = 0.2
# Hang = 1.5 × word gap = 10.5 × dit (docs/TX_INHIBIT.md §3.4); WPM ~10..40
HANG_MIN_S = 0.315
HANG_MAX_S = 1.260
HANG_DIT_MULT = 10.5
CONTINUOUS_MARK_S = 0.5  # non-break-in / SSB → hang 0

# NetworkMessage framing. Schema comes from the Heartbeat, not a fixed number.
_NM_MAGIC = 0xADBCCBDA
_NM_HEARTBEAT = 0
_NM_INHIBIT_STATUS = 17
_NM_TX_INHIBIT = 18


def _qbytearray(data: bytes) -> bytes:
    """QDataStream QByteArray: quint32 length + bytes (empty length 0)."""
    return struct.pack(">I", len(data)) + data


def encode(
    controller_id: str,
    ttl_ms: int,
    station: str = "",
    target_id: str = "",
    schema: int = 2,
) -> bytes:
    """Build NetworkMessage::TxInhibit (type 18) for the heartbeat source."""
    if not controller_id:
        raise ValueError("controller_id must be non-empty")
    body = struct.pack(">III", _NM_MAGIC, int(schema) & 0xFFFFFFFF, _NM_TX_INHIBIT)
    body += _qbytearray(target_id.encode("utf-8"))
    body += _qbytearray(controller_id.encode("utf-8"))
    body += struct.pack(">I", int(ttl_ms) & 0xFFFFFFFF)
    body += _qbytearray(station.encode("utf-8"))
    return body


def _read_qba(buf: bytes, off: int):
    (n,) = struct.unpack_from(">I", buf, off)
    off += 4
    if n == 0xFFFFFFFF:
        return b"", off
    if n > 4096 or off + n > len(buf):
        raise ValueError("truncated utf8")
    return buf[off : off + n], off + n


def _parse_header(buf: bytes):
    if len(buf) < 16:
        return None
    magic, schema, typ = struct.unpack_from(">III", buf, 0)
    if magic != _NM_MAGIC:
        return None
    ident, off = _read_qba(buf, 12)
    return schema, typ, ident.decode("utf-8", "replace"), off


def _parse_type17(buf: bytes, off: int):
    supported = buf[off] != 0
    inhibited = buf[off + 1] != 0
    holder, _off = _read_qba(buf, off + 2)
    return supported, inhibited, holder.decode("utf-8", "replace")


class WsPeer:
    def __init__(self) -> None:
        self.addr: tuple[str, int] | None = None
        self.ident = ""
        self.schema = 2
        self.supported: bool | None = None
        self.inhibited: bool | None = None
        self.holder = ""

    def note(self, data: bytes, addr: tuple[str, int], want_id: str) -> None:
        parsed = _parse_header(data)
        if not parsed:
            return
        schema, typ, ident, off = parsed
        if want_id and ident != want_id:
            return
        if typ == _NM_INHIBIT_STATUS:
            try:
                supported, inhibited, holder = _parse_type17(data, off)
            except (ValueError, IndexError):
                return
            changed = (
                self.supported != supported
                or self.inhibited != inhibited
                or self.holder != holder
            )
            self.supported = supported
            self.inhibited = inhibited
            self.holder = holder
            if self.addr is None:
                self.addr = addr
                self.ident = ident
                self.schema = schema or self.schema
            if changed:
                print(
                    f"{time.strftime('%H:%M:%S')}  type17 id={ident!r} "
                    f"supported={supported} inhibited={inhibited} "
                    f"holder={holder or '-'}",
                    flush=True,
                )
            return
        if self.addr is None or typ == _NM_HEARTBEAT:
            first = self.addr is None
            self.addr = addr
            self.ident = ident
            if schema:
                self.schema = schema
            if first and typ == _NM_HEARTBEAT:
                print(
                    f"{time.strftime('%H:%M:%S')}  heartbeat id={ident!r} "
                    f"schema={self.schema} from {addr[0]}:{addr[1]}",
                    flush=True,
                )


def discover(listen_host: str, listen_port: int, want_id: str, timeout_s: float = 20.0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((listen_host, listen_port))
    sock.setblocking(False)
    print(f"listening {listen_host}:{listen_port} for Heartbeat and type 17", flush=True)
    peer = WsPeer()
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline and peer.addr is None:
        ready, _, _ = select.select([sock], [], [], 0.5)
        if not ready:
            continue
        data, addr = sock.recvfrom(8192)
        peer.note(data, addr, want_id)
    # One short extra wait so the type 17 that follows a heartbeat is seen.
    extra = time.monotonic() + 2.0
    while time.monotonic() < extra:
        ready, _, _ = select.select([sock], [], [], 0.2)
        if not ready:
            continue
        data, addr = sock.recvfrom(8192)
        peer.note(data, addr, want_id)
    if peer.addr is None:
        raise SystemExit(
            f"no Heartbeat on {listen_host}:{listen_port}. "
            "Point WS's UDP server at that address."
        )
    if peer.supported is False:
        print(
            "type 17 says not supported "
            "(PTT must be RTS or DTR, and Accept UDP requests must be on)",
            file=sys.stderr,
        )
    return peer, sock


def send_to(sock: socket.socket, host: str, port: int, payload: bytes) -> None:
    sock.sendto(payload, (host, port))


def one_shot(args: argparse.Namespace, peer: WsPeer, sock: socket.socket) -> None:
    assert peer.addr is not None
    msg = encode(
        args.controller_id, args.ttl_ms, args.station or args.controller_id,
        peer.ident, peer.schema,
    )
    send_to(sock, peer.addr[0], peer.addr[1], msg)
    print(
        f"sent type-18 id={peer.ident!r} controller={args.controller_id!r} "
        f"ttl_ms={args.ttl_ms} ({len(msg)} bytes) -> {peer.addr[0]}:{peer.addr[1]}"
    )


class HangPolicy:
    def __init__(self) -> None:
        self.dit_s = 0.0

    def hang_s_for_closure(self, closure_s: float) -> float:
        """Break-in: 1.5×word gap from dit estimate. Continuous KEY: 0."""
        if closure_s <= 0:
            return 0.0
        if closure_s >= CONTINUOUS_MARK_S:
            return 0.0  # non-break-in CW / SSB
        if self.dit_s <= 0:
            self.dit_s = closure_s
        else:
            self.dit_s = 0.35 * closure_s + 0.65 * self.dit_s
        hang = HANG_DIT_MULT * self.dit_s
        return max(HANG_MIN_S, min(HANG_MAX_S, hang))


def _space_level_win() -> bool | None:
    try:
        import ctypes
    except ImportError:
        return None
    VK_OEM_3 = 0xC0  # US keyboard `~ (grave)
    return bool(ctypes.windll.user32.GetAsyncKeyState(VK_OEM_3) & 0x8000)


def _quit_win() -> bool:
    try:
        import ctypes
    except ImportError:
        return False
    user32 = ctypes.windll.user32
    return bool(user32.GetAsyncKeyState(ord("Q")) & 0x8000) or bool(
        user32.GetAsyncKeyState(0x1B) & 0x8000
    )


_evdev_fd = None


def _open_evdev() -> bool:
    global _evdev_fd
    if _evdev_fd is not None:
        return _evdev_fd >= 0
    _evdev_fd = -1
    import glob
    import struct

    candidates = sorted(glob.glob("/dev/input/by-path/*-event-kbd")) + sorted(
        glob.glob("/dev/input/event*")
    )
    # EVIOCGKEY size: KEY_MAX is typically 767 → ~96 bytes; use 256
    EVIOCGKEY = 0x80004518 | (256 << 16)  # rough; use fcntl with correct size below
    for path in candidates:
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
        except OSError:
            continue
        # EVIOCGBIT(0) check skipped for brevity; try EVIOCGKEY
        buf = bytearray(128)
        try:
            import fcntl
            # EVIOCGKEY(len) = _IOC(_IOC_READ, 'E', 0x18, len)
            req = 0x80004518 | (len(buf) << 16)
            fcntl.ioctl(fd, req, buf)
        except OSError:
            os.close(fd)
            continue
        _evdev_fd = fd
        return True
    return False


def _space_level_linux() -> bool | None:
    if not _open_evdev():
        return None
    import fcntl

    # Drain
    try:
        while os.read(_evdev_fd, 64):
            pass
    except BlockingIOError:
        pass
    except OSError:
        return None
    buf = bytearray(128)
    req = 0x80004518 | (len(buf) << 16)
    try:
        fcntl.ioctl(_evdev_fd, req, buf)
    except OSError:
        return None
    # KEY_GRAVE = 41 (backtick / left quote)
    bit = 41
    return bool(buf[bit // 8] & (1 << (bit % 8)))


def space_down() -> bool | None:
    """True/False if level available; None if no platform reader."""
    if sys.platform == "win32":
        return _space_level_win()
    if sys.platform.startswith("linux"):
        return _space_level_linux()
    return None


def interactive(args: argparse.Namespace, peer: WsPeer, sock: socket.socket) -> None:
    assert peer.addr is not None
    seq = args.seq
    hang = HangPolicy()
    key_down = False
    band_held = False
    key_down_at = 0.0
    hang_until = -1.0
    fixed_hang = args.fixed_hang_ms
    use_fixed = fixed_hang is not None
    station = args.station or args.controller_id

    def emit(ttl_ms: int) -> None:
        nonlocal seq
        payload = encode(args.controller_id, ttl_ms, station, peer.ident, peer.schema)
        send_to(sock, peer.addr[0], peer.addr[1], payload)
        seq += 1
        action = "HOLD" if ttl_ms else "RELEASE"
        print(
            f"{time.strftime('%H:%M:%S')}  {action}  ttl_ms={ttl_ms}  "
            f"id={peer.ident!r} controller={args.controller_id!r} "
            f"-> {peer.addr[0]}:{peer.addr[1]}",
            flush=True,
        )

    def drain() -> None:
        while True:
            ready, _, _ = select.select([sock], [], [], 0)
            if not ready:
                return
            data, addr = sock.recvfrom(8192)
            peer.note(data, addr, args.target_id)

    level = space_down()
    if level is None:
        print(
            "No KEY (grave/`) level reader on this platform (need Windows or Linux "
            "/dev/input). Use the built inhibit-test binary, or fix input access.",
            file=sys.stderr,
        )
        sys.exit(1)

    print(
        f"Interactive KEY (level) → type 18 {peer.addr[0]}:{peer.addr[1]} "
        f"id={peer.ident!r} schema={peer.schema}\n"
        f"  grave/`  = KEY level (press = hold, release = hang then free)\n"
        f"  (not Space — typing won't false-trigger)\n"
        f"  Ctrl-C    = quit\n"
        f"  controller={args.controller_id!r}  station={station!r}  "
        f"ttl_ms={args.ttl_ms}  "
        f"hang={'fixed '+str(fixed_hang)+'ms' if use_fixed else 'adaptive'}\n",
        flush=True,
    )

    try:
        while True:
            now = time.monotonic()
            drain()
            space = bool(space_down())

            if space and not key_down:
                key_down = True
                key_down_at = now
                hang_until = -1.0
                if not band_held:
                    band_held = True
                    emit(args.ttl_ms)
                print(f"{now:10.3f}  KEY DOWN", flush=True)

            if not space and key_down:
                key_down = False
                closure = now - key_down_at
                if use_fixed:
                    hang_s = max(0.0, (fixed_hang or 0) / 1000.0)
                else:
                    hang_s = hang.hang_s_for_closure(closure)
                hang_until = now + hang_s
                dit = f"  dit≈{hang.dit_s*1000:.0f} ms" if hang.dit_s > 0 else ""
                print(
                    f"{now:10.3f}  KEY UP    closure={closure*1000:.0f} ms  "
                    f"hang={hang_s*1000:.0f} ms{dit}",
                    flush=True,
                )

            if not key_down and hang_until >= 0 and now >= hang_until:
                hang_until = -1.0
                if band_held:
                    emit(0)
                    band_held = False
                    print(f"{now:10.3f}  RELEASE (hang done)", flush=True)

            if band_held and (now - getattr(interactive, "_last_ka", 0)) >= KEEPALIVE_S:
                emit(args.ttl_ms)
                interactive._last_ka = now  # type: ignore[attr-defined]

            if band_held and not hasattr(interactive, "_last_ka"):
                interactive._last_ka = now  # type: ignore[attr-defined]

            time.sleep(0.01)
    except KeyboardInterrupt:
        if band_held:
            emit(0)
        print("\nquit (Ctrl-C)")


def main() -> None:
    p = argparse.ArgumentParser(
        description="Send TX Inhibit hold/release UDP datagrams to wsjtx-inhibit"
    )
    p.add_argument("--host", default="127.0.0.1",
                   help="Address to bind. WS's UDP server setting.")
    p.add_argument(
        "--port",
        type=int,
        default=2237,
        help="UDP server port WS sends Heartbeat and type 17 to (default 2237)",
    )
    p.add_argument(
        "--ttl-ms",
        type=int,
        default=DEFAULT_TTL_MS,
        help="Hold TTL ms; 0 = release (one-shot). Interactive: keepalive TTL.",
    )
    p.add_argument(
        "--controller-id",
        default="TEST-SSB",
        help="Lease Controller ID (required non-empty on the wire)",
    )
    p.add_argument(
        "--station",
        default=None,
        help="Badge station text (default: controller-id)",
    )
    p.add_argument(
        "--target-id",
        default="",
        help="Only this WS instance id (default: first heartbeat)",
    )
    p.add_argument("--seq", type=int, default=1)
    p.add_argument(
        "-i",
        "--interactive",
        action="store_true",
        help="Grave/` KEY level + hang (docs §3)",
    )
    p.add_argument(
        "--fixed-hang-ms",
        type=int,
        default=None,
        help="Interactive: fixed hang after KEY up instead of adaptive",
    )
    args = p.parse_args()
    if not args.controller_id:
        p.error("controller-id must be non-empty")
    peer, sock = discover(args.host, args.port, args.target_id)
    if args.interactive:
        interactive(args, peer, sock)
    else:
        one_shot(args, peer, sock)


if __name__ == "__main__":
    main()
