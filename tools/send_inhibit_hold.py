#!/usr/bin/env python3
"""Send a WIMS TX-inhibit hold (or release) to a local gate."""
import argparse, json, socket

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=22372)
    p.add_argument("--ttl-ms", type=int, default=600,
                   help="0 = release")
    p.add_argument("--station", default="TEST-SSB")
    p.add_argument("--band", default="144")
    p.add_argument("--seq", type=int, default=1)
    args = p.parse_args()
    msg = json.dumps({
        "tx_inhibit": 1,
        "ttl_ms": args.ttl_ms,
        "station": args.station,
        "band": args.band,
        "seq": args.seq,
    }, separators=(",", ":")).encode()
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.sendto(msg, (args.host, args.port))
    print(f"sent {msg!r} -> {args.host}:{args.port}")

if __name__ == "__main__":
    main()
