#!/usr/bin/env python3
"""Comparison plots: RX vs TX, CPU quiet vs compile load."""
from __future__ import annotations

import csv
import datetime as dt
from collections import defaultdict
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

CSV = Path("/home/jeff/ham/wsjtx-32rc1-latency/inhibit-latency-hist.csv")
OUT = Path("/home/jeff/ham/wsjtx-inhibit/docs/pr61-review")
COMPILE = dt.datetime(2026, 9, 25, 11, 53, 4).timestamp()
BUDGET_US = 30_000.0
TITLE = "3.2.0-rc1 TX Inhibit"

COL_RX = (31, 119, 180)
COL_TX = (214, 39, 40)
COL_QUIET = (44, 160, 44)
COL_BUSY = (255, 127, 14)
BG = (255, 255, 255)
INK = (30, 30, 30)
GRID = (220, 220, 220)
BUDGET = (180, 0, 0)


def pct(v, p):
    if v.size == 0:
        return float("nan")
    s = np.sort(v)
    i = int(round((len(s) - 1) * p))
    return float(s[min(i, len(s) - 1)])


def load():
    t, w, lat = [], [], []
    with CSV.open() as f:
        for r in csv.DictReader(f):
            t.append(float(r["t_unix"]))
            w.append(r["window"])
            lat.append(float(r["lat_status_us"]))
    t = np.array(t)
    lat = np.array(lat)
    w = np.array(w)
    rx = w != "tx"
    tx = w == "tx"
    quiet = t < COMPILE
    busy = t >= COMPILE
    return {
        "t": t,
        "lat": lat,
        "rx": lat[rx],
        "tx": lat[tx],
        "rx_quiet": lat[rx & quiet],
        "rx_busy": lat[rx & busy],
        "tx_quiet": lat[tx & quiet],
        "tx_busy": lat[tx & busy],
        "t0": t.min(),
        "is_tx": tx,
        "quiet": quiet,
    }


def font():
    for p in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ):
        if Path(p).exists():
            return ImageFont.truetype(p, 16), ImageFont.truetype(p, 22), ImageFont.truetype(p, 13)
    f = ImageFont.load_default()
    return f, f, f


F, F_TITLE, F_SMALL = font()


def hist(vals, lo, hi, nbin):
    if vals.size == 0:
        return np.zeros(nbin), np.linspace(lo, hi, nbin + 1)
    counts, edges = np.histogram(vals, bins=nbin, range=(lo, hi))
    return counts.astype(float), edges


def panel(d, box, title, series, lo, hi, nbin, y_max=None, vlines=()):
    """series: list of (label, values, color). box = (x0,y0,x1,y1)"""
    x0, y0, x1, y1 = box
    d.rectangle([x0, y0, x1, y1], outline=INK, fill=BG)
    plot_w, plot_h = x1 - x0, y1 - y0
    all_counts = []
    drawn = []
    for label, vals, col in series:
        c, edges = hist(vals, lo, hi, nbin)
        all_counts.append(c)
        drawn.append((label, vals, col, c, edges))
    stacked = np.sum(all_counts, axis=0) if all_counts else np.array([1.0])
    ymax = y_max or max(float(stacked.max()), 1.0)
    # grid
    for frac in (0.25, 0.5, 0.75):
        y = y1 - frac * plot_h
        d.line([x0, y, x1, y], fill=GRID)
    # bars (grouped if 2 series, stacked if 1)
    nser = len(drawn)
    for i in range(nbin):
        for s, (label, vals, col, c, edges) in enumerate(drawn):
            if c[i] <= 0:
                continue
            h = plot_h * (c[i] / ymax)
            if nser == 1:
                bx0 = x0 + i * plot_w / nbin
                bx1 = x0 + (i + 1) * plot_w / nbin
            else:
                slot = plot_w / nbin
                bw = slot / (nser + 0.3)
                bx0 = x0 + i * slot + s * bw + 0.15 * bw
                bx1 = bx0 + bw
            d.rectangle([bx0, y1 - h, bx1, y1], fill=col)
    for xv, col, lab in vlines:
        if lo <= xv <= hi:
            x = x0 + (xv - lo) / (hi - lo) * plot_w
            d.line([x, y0, x, y1], fill=col, width=2)
            d.text((x + 4, y0 + 4), lab, fill=col, font=F_SMALL)
    d.text((x0, y0 - 22), title, fill=INK, font=F)
    # x ticks
    nt = 6
    for k in range(nt + 1):
        xv = lo + (hi - lo) * k / nt
        x = x0 + k * plot_w / nt
        d.line([x, y1, x, y1 + 5], fill=INK)
        lab = f"{xv/1000:.0f}" if hi >= 4000 else f"{xv:.0f}"
        if hi >= 4000:
            lab = f"{xv/1000:.0f}"
        d.text((x - 8, y1 + 7), lab, fill=INK, font=F_SMALL)
    unit = "ms" if hi >= 4000 else "us"
    d.text((x0 + plot_w / 2 - 20, y1 + 24), f"latency ({unit if hi < 4000 else 'ms'})", fill=INK, font=F_SMALL)
    d.text((x0 - 36, y1 - plot_h - 2), f"{int(ymax)}", fill=INK, font=F_SMALL)
    d.text((x0 - 36, y1 - 10), "0", fill=INK, font=F_SMALL)


def legend(d, x, y, items):
    for i, (lab, col) in enumerate(items):
        yy = y + i * 20
        d.rectangle([x, yy, x + 16, yy + 12], fill=col)
        d.text((x + 22, yy - 2), lab, fill=INK, font=F_SMALL)


def stats_line(vals):
    if vals.size == 0:
        return "n=0"
    return (
        f"n={vals.size}  min={vals.min()/1000:.2f}  p50={pct(vals,0.5)/1000:.2f}  "
        f"p95={pct(vals,0.95)/1000:.2f}  max={vals.max()/1000:.2f} ms"
    )


def plot_rx_tx(data, path):
    W, H = 1400, 980
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((40, 18), f"{TITLE}  —  Receiving vs Transmitting", fill=INK, font=F_TITLE)
    d.text((40, 48), "type-18 send → type-17 Inhibited=true   red dashed = 30 ms budget", fill=INK, font=F)
    pad = 80
    panel(
        d, (pad, 110, W - 40, 430),
        f"Receiving (idle+decode)   {stats_line(data['rx'])}",
        [("RX", data["rx"], COL_RX)],
        0, 30_000, 60,
        vlines=[(BUDGET_US, BUDGET, "30 ms")],
    )
    panel(
        d, (pad, 520, W - 40, 860),
        f"Transmitting   {stats_line(data['tx'])}",
        [("TX", data["tx"], COL_TX)],
        0, 250_000, 50,
        vlines=[(BUDGET_US, BUDGET, "30 ms")],
    )
    d.text((40, 900), "RX zoomed to the 30 ms budget. TX uses 0–250 ms so a mid-TX floor is visible.", fill=INK, font=F)
    d.text((40, 924), "Times are type-18 send → type-17 Inhibited=true (GUI announce), not RTS fall.", fill=INK, font=F_SMALL)
    im.save(path)
    print("wrote", path)


def plot_cpu(data, path):
    W, H = 1400, 1000
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((40, 16), f"{TITLE}  —  CPU quiet vs compile load", fill=INK, font=F_TITLE)
    d.text((40, 46), "Quiet: before 11:53:04 (make -j8). Busy: after. Same 30 ms budget line.", fill=INK, font=F)
    # 2x2
    # RX quiet | RX busy
    # TX quiet | TX busy
    m = 70
    gap = 50
    top = 90
    pw = (W - 80 - gap) // 2
    ph = 340
    cells = [
        (m, top, "RX, CPU quiet", data["rx_quiet"], COL_RX, 0, 30_000, 60),
        (m + pw + gap, top, "RX, CPU busy (compile)", data["rx_busy"], COL_BUSY, 0, 30_000, 60),
        (m, top + ph + 90, "TX, CPU quiet", data["tx_quiet"], COL_TX, 100_000, 250_000, 40),
        (m + pw + gap, top + ph + 90, "TX, CPU busy (compile)", data["tx_busy"], COL_BUSY, 100_000, 250_000, 40),
    ]
    for x, y, title, vals, col, lo, hi, nbin in cells:
        panel(
            d, (x, y + 28, x + pw, y + 28 + ph),
            f"{title}   {stats_line(vals)}",
            [(title, vals, col)],
            lo, hi, nbin,
            vlines=[(BUDGET_US, BUDGET, "30 ms")] if hi <= 40_000 else [],
        )
    d.text((40, 960), "Compile load raises RX p95 (4 ms → ~19 ms) and a few outliers. TX floor stays ~163–170 ms either side.", fill=INK, font=F)
    im.save(path)
    print("wrote", path)


def plot_timeline(data, path):
    W, H = 1400, 720
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((40, 16), "Latency vs wall time  (log scale)   blue=RX  red=TX   orange line=compile start", fill=INK, font=F_TITLE)
    x0, y0, x1, y1 = 80, 70, 1360, 640
    d.rectangle([x0, y0, x1, y1], outline=INK)
    t = data["t"]
    lat = data["lat"]
    t0, t1 = t.min(), t.max()
    # log y 200 us .. 400000 us
    ylo, yhi = np.log10(200.0), np.log10(400_000.0)

    def X(tv):
        return x0 + (tv - t0) / (t1 - t0) * (x1 - x0)

    def Y(us):
        us = min(max(us, 200.0), 400_000.0)
        return y1 - (np.log10(us) - ylo) / (yhi - ylo) * (y1 - y0)

    for decade in (3, 4, 5):  # 1ms, 10ms, 100ms
        y = Y(10 ** decade)
        d.line([x0, y, x1, y], fill=GRID)
        d.text((18, y - 8), f"{10**(decade-3):.0f} ms" if decade >= 3 else "", fill=INK, font=F_SMALL)
    d.line([x0, Y(BUDGET_US), x1, Y(BUDGET_US)], fill=BUDGET, width=2)
    d.text((x1 - 70, Y(BUDGET_US) - 16), "30 ms", fill=BUDGET, font=F_SMALL)
    xc = X(COMPILE)
    d.line([xc, y0, xc, y1], fill=COL_BUSY, width=2)
    d.text((xc + 6, y0 + 6), "compile", fill=COL_BUSY, font=F_SMALL)
    # points
    for i in range(len(t)):
        col = COL_TX if data["is_tx"][i] else COL_RX
        x, y = X(t[i]), Y(lat[i])
        d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=col)
    # x labels every minute
    start = dt.datetime.fromtimestamp(t0).replace(second=0, microsecond=0)
    tt = start.timestamp()
    while tt < t1:
        if tt >= t0:
            x = X(tt)
            d.line([x, y1, x, y1 + 5], fill=INK)
            d.text((x - 18, y1 + 8), dt.datetime.fromtimestamp(tt).strftime("%H:%M"), fill=INK, font=F_SMALL)
        tt += 60
    legend(d, 90, 78, [("Receiving", COL_RX), ("Transmitting", COL_TX)])
    im.save(path)
    print("wrote", path)


def plot_phase(data, path):
    # reload phase
    phase_rx = defaultdict(list)
    phase_tx = defaultdict(list)
    with CSV.open() as f:
        for r in csv.DictReader(f):
            ph = int(float(r["phase_ms"])) // 500  # 0.5 s bins
            lat = float(r["lat_status_us"])
            if r["window"] == "tx":
                phase_tx[ph].append(lat)
            else:
                phase_rx[ph].append(lat)
    W, H = 1400, 720
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((40, 16), "p50 latency vs FT8 15 s phase   (0.5 s bins)   blue=RX  red=TX", fill=INK, font=F_TITLE)
    x0, y0, x1, y1 = 80, 70, 1360, 640
    d.rectangle([x0, y0, x1, y1], outline=INK)
    ylo, yhi = np.log10(200.0), np.log10(400_000.0)

    def X(ms):
        return x0 + ms / 15000.0 * (x1 - x0)

    def Y(us):
        us = min(max(us, 200.0), 400_000.0)
        return y1 - (np.log10(us) - ylo) / (yhi - ylo) * (y1 - y0)

    for decade in (3, 4, 5):
        y = Y(10 ** decade)
        d.line([x0, y, x1, y], fill=GRID)
        d.text((18, y - 8), f"{10**(decade-3):.0f} ms", fill=INK, font=F_SMALL)
    d.line([x0, Y(BUDGET_US), x1, Y(BUDGET_US)], fill=BUDGET, width=2)
    d.text((x1 - 70, Y(BUDGET_US) - 16), "30 ms", fill=BUDGET, font=F_SMALL)

    def polyline(bins, col):
        pts = []
        for b in range(30):
            v = np.array(bins.get(b, []), dtype=float)
            if v.size < 3:
                continue
            pts.append((X(b * 500 + 250), Y(pct(v, 0.5))))
        for a in range(len(pts) - 1):
            d.line([pts[a][0], pts[a][1], pts[a + 1][0], pts[a + 1][1]], fill=col, width=3)
            d.ellipse([pts[a][0] - 3, pts[a][1] - 3, pts[a][0] + 3, pts[a][1] + 3], fill=col)
        if pts:
            x, y = pts[-1]
            d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=col)

    polyline(phase_rx, COL_RX)
    polyline(phase_tx, COL_TX)
    for s in (0, 5, 10, 15):
        x = X(s * 1000)
        d.line([x, y1, x, y1 + 5], fill=INK)
        d.text((x - 8, y1 + 8), f"{s}s", fill=INK, font=F_SMALL)
    legend(d, 90, 78, [("RX p50", COL_RX), ("TX p50", COL_TX)])
    d.text((40, 670), "RX p50 stays ~2–6 ms across the cycle. TX p50 is ~170 ms wherever TX samples exist.", fill=INK, font=F)
    im.save(path)
    print("wrote", path)


def main():
    import argparse
    global CSV, OUT, COMPILE, TITLE
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=str(CSV))
    ap.add_argument("--out-dir", default=str(OUT))
    ap.add_argument("--compile-unix", type=float, default=COMPILE)
    ap.add_argument("--title", default=TITLE)
    ap.add_argument("--prefix", default="compare")
    args = ap.parse_args()
    CSV = Path(args.csv)
    OUT = Path(args.out_dir)
    COMPILE = args.compile_unix
    TITLE = args.title
    OUT.mkdir(parents=True, exist_ok=True)
    data = load()
    plot_rx_tx(data, OUT / f"{args.prefix}-rx-tx.png")
    plot_cpu(data, OUT / f"{args.prefix}-cpu-load.png")
    plot_timeline(data, OUT / f"{args.prefix}-timeline.png")
    plot_phase(data, OUT / f"{args.prefix}-phase.png")


if __name__ == "__main__":
    main()
