#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""TX Inhibit spacebar tester — GUI for wsjtx-inhibit / WIMS protocol.

Hold SPACE (or the on-screen button) to send UDP inhibit holds/keepalives
to a local or remote wsjtx-inhibit gate. Release to send ttl_ms=0.

Protocol matches tools/send_inhibit_hold.py and TxInhibitLogic.hpp:
  {"tx_inhibit":1,"ttl_ms":N,"station":"...","band":"...","seq":N}

No third-party packages — uses tkinter (bundled with python.org and most
MSYS/python installs on Windows).

Usage:
  py -3 tools/inhibit_spacebar_gui.py
  python3 tools/inhibit_spacebar_gui.py --host 127.0.0.1 --port 22372
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
import tkinter as tk
from tkinter import ttk
from typing import Optional

PROTOCOL_KEY = "tx_inhibit"
PROTOCOL_VERSION = 1
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 22372
DEFAULT_TTL_MS = 600
DEFAULT_KEEPALIVE_MS = 200
TTL_MS_MIN, TTL_MS_MAX = 100, 30_000


def encode_datagram(station: str, band: str, seq: int, ttl_ms: int) -> bytes:
    ttl_ms = int(ttl_ms)
    if ttl_ms != 0 and not (TTL_MS_MIN <= ttl_ms <= TTL_MS_MAX):
        raise ValueError(f"ttl_ms must be 0 or {TTL_MS_MIN}..{TTL_MS_MAX}, got {ttl_ms}")
    payload = {
        PROTOCOL_KEY: PROTOCOL_VERSION,
        "ttl_ms": ttl_ms,
        "station": station,
        "band": band,
        "seq": int(seq),
    }
    return json.dumps(payload, separators=(",", ":")).encode("ascii")


class InhibitSpacebarApp(tk.Tk):
    def __init__(
        self,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT,
        station: str = "TEST-SSB",
        band: str = "144",
        ttl_ms: int = DEFAULT_TTL_MS,
        keepalive_ms: int = DEFAULT_KEEPALIVE_MS,
    ):
        super().__init__()
        self.title("wsjtx-inhibit — Spacebar TX Inhibit Tester")
        self.minsize(480, 420)
        self.geometry("560x480")

        # Config vars
        self.var_host = tk.StringVar(value=host)
        self.var_port = tk.StringVar(value=str(port))
        self.var_station = tk.StringVar(value=station)
        self.var_band = tk.StringVar(value=band)
        self.var_ttl = tk.StringVar(value=str(ttl_ms))
        self.var_keepalive = tk.StringVar(value=str(keepalive_ms))

        # Runtime status
        self.var_state = tk.StringVar(value="IDLE")
        self.var_last_action = tk.StringVar(value="—")
        self.var_last_payload = tk.StringVar(value="—")
        self.var_last_error = tk.StringVar(value="")
        self.var_seq = tk.StringVar(value="0")
        self.var_holds = tk.StringVar(value="0")
        self.var_releases = tk.StringVar(value="0")
        self.var_keepalives = tk.StringVar(value="0")
        self.var_errors = tk.StringVar(value="0")
        self.var_deadline = tk.StringVar(value="—")
        self.var_hint = tk.StringVar(
            value="Hold SPACE (or press & hold the big button) to INHIBIT. Release to clear."
        )

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setblocking(False)
        self._seq = 0
        self._holds = 0
        self._releases = 0
        self._keepalives = 0
        self._errors = 0
        self._holding = False
        self._space_down = False
        self._deadline_mono: Optional[float] = None
        self._ka_after: Optional[str] = None
        self._tick_after: Optional[str] = None

        self._build_ui()
        self._bind_keys()
        self._schedule_tick()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── UI ────────────────────────────────────────────────────────────
    def _build_ui(self) -> None:
        pad = {"padx": 8, "pady": 4}
        root = ttk.Frame(self, padding=10)
        root.pack(fill=tk.BOTH, expand=True)

        # Target
        tgt = ttk.LabelFrame(root, text="Target (wsjtx-inhibit gate)", padding=8)
        tgt.pack(fill=tk.X, **pad)
        self._row(tgt, 0, "Host", self.var_host, 18)
        self._row(tgt, 0, "Port", self.var_port, 8, col=2)
        self._row(tgt, 1, "Station", self.var_station, 18)
        self._row(tgt, 1, "Band", self.var_band, 8, col=2)
        self._row(tgt, 2, "TTL ms", self.var_ttl, 8)
        self._row(tgt, 2, "Keepalive ms", self.var_keepalive, 8, col=2)

        # Big hold button
        btn_fr = ttk.Frame(root)
        btn_fr.pack(fill=tk.BOTH, expand=True, **pad)
        self.hold_btn = tk.Button(
            btn_fr,
            text="HOLD TO INHIBIT\n(spacebar or mouse)",
            font=("Segoe UI", 16, "bold"),
            bg="#2e7d32",
            fg="white",
            activebackground="#1b5e20",
            activeforeground="white",
            relief=tk.RAISED,
            bd=4,
            height=4,
        )
        self.hold_btn.pack(fill=tk.BOTH, expand=True)
        self.hold_btn.bind("<ButtonPress-1>", self._on_hold_press)
        self.hold_btn.bind("<ButtonRelease-1>", self._on_hold_release)
        self.hold_btn.bind("<Leave>", self._on_hold_leave)

        # Status
        st = ttk.LabelFrame(root, text="Status", padding=8)
        st.pack(fill=tk.X, **pad)

        self.state_label = ttk.Label(
            st, textvariable=self.var_state, font=("Segoe UI", 14, "bold")
        )
        self.state_label.grid(row=0, column=0, columnspan=4, sticky=tk.W, pady=(0, 6))

        self._stat(st, 1, 0, "Last action", self.var_last_action)
        self._stat(st, 1, 2, "Hold deadline", self.var_deadline)
        self._stat(st, 2, 0, "Seq", self.var_seq)
        self._stat(st, 2, 2, "Holds sent", self.var_holds)
        self._stat(st, 3, 0, "Keepalives", self.var_keepalives)
        self._stat(st, 3, 2, "Releases", self.var_releases)
        self._stat(st, 4, 0, "Send errors", self.var_errors)

        ttk.Label(st, text="Last packet:").grid(row=5, column=0, sticky=tk.NW, pady=(6, 0))
        ttk.Label(st, textvariable=self.var_last_payload, wraplength=480, justify=tk.LEFT).grid(
            row=5, column=1, columnspan=3, sticky=tk.W, pady=(6, 0)
        )
        ttk.Label(st, textvariable=self.var_last_error, foreground="#b71c1c", wraplength=500).grid(
            row=6, column=0, columnspan=4, sticky=tk.W, pady=(4, 0)
        )

        # One-shot release + hint
        bot = ttk.Frame(root)
        bot.pack(fill=tk.X, **pad)
        ttk.Button(bot, text="Send release now (ttl=0)", command=self._send_release_click).pack(
            side=tk.LEFT
        )
        ttk.Label(bot, textvariable=self.var_hint, wraplength=320).pack(
            side=tk.LEFT, padx=12
        )

        for c in range(4):
            st.columnconfigure(c, weight=1)

    def _row(
        self, parent, row, label, var, width, col: int = 0
    ) -> None:
        ttk.Label(parent, text=label + ":").grid(row=row, column=col, sticky=tk.E, padx=(0, 4))
        ttk.Entry(parent, textvariable=var, width=width).grid(
            row=row, column=col + 1, sticky=tk.W, padx=(0, 12)
        )

    def _stat(self, parent, row, col, label, var) -> None:
        ttk.Label(parent, text=label + ":").grid(row=row, column=col, sticky=tk.E, padx=(0, 4))
        ttk.Label(parent, textvariable=var, font=("Consolas", 10)).grid(
            row=row, column=col + 1, sticky=tk.W, padx=(0, 12)
        )

    def _bind_keys(self) -> None:
        # Bind on the toplevel so space works without focusing the button
        self.bind_all("<KeyPress-space>", self._on_space_press)
        self.bind_all("<KeyRelease-space>", self._on_space_release)
        # Avoid space activating focused buttons repeatedly
        self.bind_all("<KeyPress-Escape>", lambda e: self._force_release())

    # ── Hold / release ────────────────────────────────────────────────
    def _on_space_press(self, event=None) -> str:
        # Ignore auto-repeat: only first press starts hold
        if self._space_down:
            return "break"
        w = self.focus_get()
        if isinstance(w, (ttk.Entry, tk.Entry)):
            return  # typing in a field
        self._space_down = True
        self._start_hold()
        return "break"

    def _on_space_release(self, event=None) -> str:
        if not self._space_down:
            return "break"
        self._space_down = False
        self._end_hold()
        return "break"

    def _on_hold_press(self, event=None) -> None:
        self._start_hold()

    def _on_hold_release(self, event=None) -> None:
        self._end_hold()

    def _on_hold_leave(self, event=None) -> None:
        # If mouse leaves while button down, still release (safer deadman)
        if self._holding and not self._space_down:
            self._end_hold()

    def _force_release(self) -> None:
        self._space_down = False
        if self._holding:
            self._end_hold()

    def _start_hold(self) -> None:
        if self._holding:
            return
        self._holding = True
        self._set_state_ui(True)
        ok = self._send_hold(is_keepalive=False)
        if ok:
            self._schedule_keepalive()

    def _end_hold(self) -> None:
        if not self._holding:
            return
        self._holding = False
        self._cancel_keepalive()
        self._send_release()
        self._set_state_ui(False)

    def _set_state_ui(self, holding: bool) -> None:
        if holding:
            self.var_state.set("INHIBITING (hold active)")
            self.state_label.configure(foreground="#b71c1c")
            self.hold_btn.configure(bg="#c62828", activebackground="#b71c1c", text="INHIBITING…\nrelease space / mouse")
        else:
            self.var_state.set("IDLE (open)")
            self.state_label.configure(foreground="#1b5e20")
            self.hold_btn.configure(
                bg="#2e7d32",
                activebackground="#1b5e20",
                text="HOLD TO INHIBIT\n(spacebar or mouse)",
            )

    # ── Networking ────────────────────────────────────────────────────
    def _target(self) -> tuple[str, int]:
        host = self.var_host.get().strip() or DEFAULT_HOST
        try:
            port = int(self.var_port.get().strip())
        except ValueError as e:
            raise ValueError("Port must be an integer") from e
        if not (1 <= port <= 65535):
            raise ValueError("Port must be 1..65535")
        return host, port

    def _ttl(self) -> int:
        try:
            ttl = int(self.var_ttl.get().strip())
        except ValueError as e:
            raise ValueError("TTL ms must be an integer") from e
        if not (TTL_MS_MIN <= ttl <= TTL_MS_MAX):
            raise ValueError(f"TTL ms must be {TTL_MS_MIN}..{TTL_MS_MAX}")
        return ttl

    def _ka_ms(self) -> int:
        try:
            ms = int(self.var_keepalive.get().strip())
        except ValueError:
            return DEFAULT_KEEPALIVE_MS
        return max(50, min(ms, 2000))

    def _send_hold(self, is_keepalive: bool) -> bool:
        try:
            host, port = self._target()
            ttl = self._ttl()
            station = self.var_station.get().strip() or "TEST-SSB"
            band = self.var_band.get().strip() or "144"
            self._seq += 1
            data = encode_datagram(station, band, self._seq, ttl)
            self._sock.sendto(data, (host, port))
        except Exception as e:
            self._errors += 1
            self.var_errors.set(str(self._errors))
            self.var_last_error.set(f"Send failed: {e}")
            return False

        now = time.monotonic()
        self._deadline_mono = now + (ttl / 1000.0)
        if is_keepalive:
            self._keepalives += 1
            self.var_keepalives.set(str(self._keepalives))
            self.var_last_action.set(f"keepalive → {host}:{port}")
        else:
            self._holds += 1
            self.var_holds.set(str(self._holds))
            self.var_last_action.set(f"HOLD → {host}:{port}")
        self.var_seq.set(str(self._seq))
        self.var_last_payload.set(data.decode("ascii"))
        self.var_last_error.set("")
        return True

    def _send_release(self) -> bool:
        try:
            host, port = self._target()
            station = self.var_station.get().strip() or "TEST-SSB"
            band = self.var_band.get().strip() or "144"
            self._seq += 1
            data = encode_datagram(station, band, self._seq, 0)
            self._sock.sendto(data, (host, port))
        except Exception as e:
            self._errors += 1
            self.var_errors.set(str(self._errors))
            self.var_last_error.set(f"Release failed: {e}")
            return False

        self._deadline_mono = None
        self._releases += 1
        self.var_releases.set(str(self._releases))
        self.var_seq.set(str(self._seq))
        self.var_last_action.set(f"RELEASE → {host}:{port}")
        self.var_last_payload.set(data.decode("ascii"))
        self.var_last_error.set("")
        self.var_deadline.set("—")
        return True

    def _send_release_click(self) -> None:
        self._space_down = False
        if self._holding:
            self._end_hold()
        else:
            self._send_release()
            self._set_state_ui(False)

    # ── Timers ────────────────────────────────────────────────────────
    def _schedule_keepalive(self) -> None:
        self._cancel_keepalive()
        self._ka_after = self.after(self._ka_ms(), self._on_keepalive)

    def _cancel_keepalive(self) -> None:
        if self._ka_after is not None:
            try:
                self.after_cancel(self._ka_after)
            except tk.TclError:
                pass
            self._ka_after = None

    def _on_keepalive(self) -> None:
        self._ka_after = None
        if not self._holding:
            return
        self._send_hold(is_keepalive=True)
        if self._holding:
            self._schedule_keepalive()

    def _schedule_tick(self) -> None:
        self._tick_after = self.after(100, self._on_tick)

    def _on_tick(self) -> None:
        if self._deadline_mono is not None:
            remaining = self._deadline_mono - time.monotonic()
            if remaining <= 0:
                self.var_deadline.set("expired (deadman)")
                if not self._holding:
                    self._deadline_mono = None
            else:
                self.var_deadline.set(f"{remaining * 1000:.0f} ms left")
        self._schedule_tick()

    def _on_close(self) -> None:
        try:
            if self._holding:
                self._send_release()
        except Exception:
            pass
        self._cancel_keepalive()
        if self._tick_after:
            try:
                self.after_cancel(self._tick_after)
            except tk.TclError:
                pass
        try:
            self._sock.close()
        except Exception:
            pass
        self.destroy()


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="GUI TX Inhibit spacebar tester")
    p.add_argument("--host", default=DEFAULT_HOST)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--station", default="TEST-SSB")
    p.add_argument("--band", default="144")
    p.add_argument("--ttl-ms", type=int, default=DEFAULT_TTL_MS)
    p.add_argument("--keepalive-ms", type=int, default=DEFAULT_KEEPALIVE_MS)
    args = p.parse_args(argv)

    try:
        import tkinter  # noqa: F401
    except ImportError:
        print(
            "tkinter is not available in this Python. On Windows use python.org install;\n"
            "on Linux: sudo apt install python3-tk",
            file=sys.stderr,
        )
        return 1

    app = InhibitSpacebarApp(
        host=args.host,
        port=args.port,
        station=args.station,
        band=args.band,
        ttl_ms=args.ttl_ms,
        keepalive_ms=args.keepalive_ms,
    )
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
