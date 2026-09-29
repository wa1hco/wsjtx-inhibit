#!/usr/bin/env python3
"""High-resolution histograms from probe_tx_inhibit_latency.py CSV."""
from __future__ import annotations

import argparse
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    Image = None


def pct(values, p):
    if not values:
        return 0.0
    s = sorted(values)
    idx = int(round((len(s) - 1) * p))
    return s[min(idx, len(s) - 1)]


def load_rows(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            try:
                lat = float(r["lat_status_us"])
                phase = int(float(r.get("phase_ms") or 0))
            except (KeyError, ValueError):
                continue
            rts = None
            try:
                if r.get("lat_rts_us"):
                    rts = float(r["lat_rts_us"])
            except ValueError:
                pass
            rows.append(
                {
                    "lat": lat,
                    "rts": rts,
                    "phase": phase,
                    "window": r.get("window") or "idle",
                }
            )
    return rows


def hist_counts(values, lo, hi, bin_w):
    nbin = max(1, int(math.ceil((hi - lo) / bin_w)))
    counts = [0] * nbin
    overflow = 0
    for v in values:
        if v < lo:
            continue
        if v >= hi:
            overflow += 1
            continue
        i = int((v - lo) / bin_w)
        counts[min(i, nbin - 1)] += 1
    return counts, overflow


def draw_latency_hist(rows, out_png, lo=0.0, hi=25000.0, bin_w=100.0):
    W, H = 1400, 900
    pad_l, pad_r, pad_t, pad_b = 80, 40, 70, 90
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)
    font = ImageFont.load_default()
    colors = {"idle": (31, 119, 180), "decode": (214, 39, 40), "tx": (44, 160, 44)}
    by = defaultdict(list)
    for r in rows:
        by[r["window"]].append(r["lat"])
    nbin = int(math.ceil((hi - lo) / bin_w))
    stacked = [0] * nbin
    series = []
    for name in ("idle", "decode", "tx"):
        c, ov = hist_counts(by.get(name, []), lo, hi, bin_w)
        series.append((name, c, ov, colors[name]))
        for i, v in enumerate(c):
            stacked[i] += v
    ymax = max(stacked) if stacked else 1
    plot_w = W - pad_l - pad_r
    plot_h = H - pad_t - pad_b
    d.rectangle([pad_l, pad_t, W - pad_r, H - pad_b], outline="black")
    # stacked bars
    bar_w = plot_w / nbin
    for i in range(nbin):
        y = H - pad_b
        x0 = pad_l + i * bar_w
        for name, c, ov, col in series:
            if c[i] <= 0:
                continue
            h = plot_h * (c[i] / ymax)
            d.rectangle([x0, y - h, x0 + bar_w, y], fill=col)
            y -= h
    # axes labels
    d.text((pad_l, 12), "Type-18 send to type-17 Inhibited  (100 us bins)", fill="black", font=font)
    d.text((pad_l, 32), f"n={len(rows)}   bin={bin_w:.0f} us   range={lo:.0f}-{hi:.0f} us", fill="black", font=font)
    for name, c, ov, col in series:
        n = sum(c)
        d.text((W - 280, 12 + 16 * ("idle", "decode", "tx").index(name)),
               f"{name}: n={n} overflow>{hi:.0f}us={ov}", fill=col, font=font)
    for frac in (0, 0.25, 0.5, 0.75, 1.0):
        y = H - pad_b - frac * plot_h
        d.line([pad_l - 4, y, pad_l, y], fill="black")
        d.text((8, y - 6), f"{int(ymax * frac)}", fill="black", font=font)
    for ms in range(0, int(hi / 1000) + 1):
        x = pad_l + (ms * 1000 - lo) / (hi - lo) * plot_w
        if pad_l <= x <= W - pad_r:
            d.line([x, H - pad_b, x, H - pad_b + 5], fill="black")
            d.text((x - 8, H - pad_b + 10), f"{ms} ms", fill="black", font=font)
    d.text((W / 2 - 40, H - 28), "latency", fill="black", font=font)
    img.save(out_png)
    print("wrote", out_png)


def draw_phase(rows, out_png, bin_ms=100):
    W, H = 1400, 900
    pad_l, pad_r, pad_t, pad_b = 80, 40, 90, 80
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)
    font = ImageFont.load_default()
    nbin = 15000 // bin_ms
    buckets = [[] for _ in range(nbin)]
    win_of = [defaultdict(int) for _ in range(nbin)]
    for r in rows:
        i = min(nbin - 1, max(0, r["phase"] // bin_ms))
        buckets[i].append(r["lat"])
        win_of[i][r["window"]] += 1
    plot_w = W - pad_l - pad_r
    plot_h = H - pad_t - pad_b
    d.rectangle([pad_l, pad_t, W - pad_r, H - pad_b], outline="black")
    # decode band ~ last 2s of RX often 13000-15000, but we paint by majority window
    ymax = 25000.0
    colors = {"idle": (31, 119, 180), "decode": (214, 39, 40), "tx": (44, 160, 44)}
    # background tint by majority window
    for i, wcounts in enumerate(win_of):
        if not wcounts:
            continue
        maj = max(wcounts, key=wcounts.get)
        x0 = pad_l + i * plot_w / nbin
        x1 = pad_l + (i + 1) * plot_w / nbin
        tint = {"idle": (235, 245, 255), "decode": (255, 235, 235), "tx": (235, 255, 235)}[maj]
        d.rectangle([x0, pad_t, x1, H - pad_b], fill=tint)
    d.rectangle([pad_l, pad_t, W - pad_r, H - pad_b], outline="black")
    def y_of(us):
        return H - pad_b - (us / ymax) * plot_h
    # p50 / p95
    xs50, ys50, xs95, ys95 = [], [], [], []
    for i, vals in enumerate(buckets):
        if len(vals) < 3:
            continue
        x = pad_l + (i + 0.5) * plot_w / nbin
        xs50.append(x); ys50.append(y_of(pct(vals, 0.50)))
        xs95.append(x); ys95.append(y_of(pct(vals, 0.95)))
        # min-max whisker
        d.line([x, y_of(min(vals)), x, y_of(max(vals))], fill=(180, 180, 180))
    def polyline(xs, ys, col, width=2):
        for a in range(len(xs) - 1):
            d.line([xs[a], ys[a], xs[a + 1], ys[a + 1]], fill=col, width=width)
    polyline(xs50, ys50, (0, 0, 0), 3)
    polyline(xs95, ys95, (214, 39, 40), 2)
    d.text((pad_l, 12), "Latency vs FT8 15 s phase   (UTC t mod 15)", fill="black", font=font)
    d.text((pad_l, 32), f"n={len(rows)}   phase bin={bin_ms} ms   black=p50  red=p95  grey=min-max", fill="black", font=font)
    d.text((pad_l, 48), "tint: blue=idle  red=decode  green=tx (majority in bin)", fill="black", font=font)
    for ms in (0, 5000, 10000, 15000):
        x = pad_l + ms / 15000 * plot_w
        d.line([x, H - pad_b, x, H - pad_b + 5], fill="black")
        d.text((x - 10, H - pad_b + 10), f"{ms/1000:.0f}s", fill="black", font=font)
    for us in (0, 5000, 10000, 15000, 20000, 25000):
        y = y_of(us)
        d.line([pad_l - 4, y, pad_l, y], fill="black")
        d.text((8, y - 6), f"{us/1000:.0f} ms", fill="black", font=font)
    img.save(out_png)
    print("wrote", out_png)


def ascii_summary(rows):
    by = defaultdict(list)
    for r in rows:
        by[r["window"]].append(r["lat"])
    print("\nLatency (us)")
    for w in ("idle", "decode", "tx"):
        v = by.get(w, [])
        if not v:
            print(f"  {w:8s} n=0")
            continue
        print(f"  {w:8s} n={len(v):4d}  min={min(v):8.0f}  p50={pct(v,0.5):8.0f}  "
              f"p95={pct(v,0.95):8.0f}  max={max(v):8.0f}")
    rts = [r["rts"] for r in rows if r.get("rts") is not None]
    if rts:
        print(f"  {'RTS fall':8s} n={len(rts):4d}  min={min(rts):8.0f}  p50={pct(rts,0.5):8.0f}  "
              f"p95={pct(rts,0.95):8.0f}  max={max(rts):8.0f}")
    # 1 ms phase bins occupancy
    nbin = 15
    print("\nSamples per 1 s of the 15 s cycle:")
    occ = [0] * nbin
    for r in rows:
        occ[min(14, r["phase"] // 1000)] += 1
    mx = max(occ) or 1
    for i, n in enumerate(occ):
        bar = "#" * int(40 * n / mx)
        print(f"  {i:2d}-{i+1:2d}s  n={n:4d}  {bar}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--out-dir", default=None)
    args = ap.parse_args()
    rows = load_rows(args.csv)
    if not rows:
        sys.exit(f"no rows in {args.csv}")
    out = Path(args.out_dir or Path(args.csv).parent)
    out.mkdir(parents=True, exist_ok=True)
    ascii_summary(rows)
    if Image is None:
        print("PIL not available; skipped PNG")
        return
    draw_latency_hist(rows, str(out / "hist-latency.png"))
    draw_phase(rows, str(out / "hist-phase.png"))


if __name__ == "__main__":
    main()
