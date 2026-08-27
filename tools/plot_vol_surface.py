#!/usr/bin/env python3
"""Plot the implied-vol surface used by the engine (data/vol_surface.csv) as a vector PDF.

Two panels:
  (left)  3-D surface: spot-moneyness x expiry x implied vol, front of the surface (<=~2y).
  (right) smile slices at a few expiries, with the accumulator's strike K and barrier H
          (as spot-moneyness) marked, to show where they sit on the skew.

Usage: python3 tools/plot_vol_surface.py [--csv data/vol_surface.csv] [--out report/fig/vol_surface_1106.pdf]
"""
import argparse, csv, os
from datetime import date
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import cm

SPOT = 34.50
K_MON = 31.0804 / SPOT * 100.0   # accrual strike as spot-moneyness %
H_MON = 37.1825 / SPOT * 100.0   # knock-out barrier as spot-moneyness %


def parse_date(s):
    y, m, d = (int(x) for x in s.strip().replace("-", "/").split("/"))
    return date(y, m, d)


def load(csv_path):
    asof = None
    grid = {}                         # expiry_date -> {moneyness: vol%}
    with open(csv_path, newline="") as f:
        for r in csv.reader(f):
            if not r or r[0] != "EQ_VS":
                continue
            if asof is None:
                asof = parse_date(r[1])
            exp = parse_date(r[18])
            grid.setdefault(exp, {})[float(r[21])] = float(r[23])
    return asof, grid


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default="data/vol_surface.csv")
    ap.add_argument("--out", default="report/fig/vol_surface_1106.pdf")
    a = ap.parse_args()

    asof, grid = load(a.csv)
    expiries = sorted(grid.keys())
    moneyness = sorted(next(iter(grid.values())).keys())
    T = {e: (e - asof).days / 365.0 for e in expiries}

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    fig = plt.figure(figsize=(11, 4.4))

    # ---- (left) 3-D surface, front of the surface (T <= ~2.2y) ----
    front = [e for e in expiries if T[e] <= 2.2]
    X, Y = np.meshgrid(moneyness, [T[e] for e in front])
    Z = np.array([[grid[e][m] for m in moneyness] for e in front])
    ax1 = fig.add_subplot(1, 2, 1, projection="3d")
    surf = ax1.plot_surface(X, Y, Z, cmap=cm.viridis, edgecolor="0.25",
                            linewidth=0.2, antialiased=True, alpha=0.95)
    ax1.set_xlabel("spot-moneyness (%)", fontsize=9, labelpad=2)
    ax1.set_ylabel("expiry $T$ (yr)", fontsize=9, labelpad=2)
    ax1.set_zlabel("implied vol (%)", fontsize=9, labelpad=2)
    ax1.set_title("(a) implied-vol surface (front $\\leq 2.2$y)", fontsize=10)
    ax1.view_init(elev=22, azim=-58)
    ax1.tick_params(labelsize=7)
    fig.colorbar(surf, ax=ax1, shrink=0.55, aspect=12, pad=0.10).ax.tick_params(labelsize=7)

    # ---- (right) smile slices at selected expiries ----
    ax2 = fig.add_subplot(1, 2, 2)
    targets = [0.08, 0.30, 0.65, 1.0]     # approx tenors (yr) to show
    picks = []
    for t in targets:
        e = min(expiries, key=lambda x: abs(T[x] - t))
        if e not in picks:
            picks.append(e)
    colors = cm.viridis(np.linspace(0.05, 0.85, len(picks)))
    for e, c in zip(picks, colors):
        vols = [grid[e][m] for m in moneyness]
        ax2.plot(moneyness, vols, "-o", color=c, ms=3, lw=1.6,
                 label=f"$T={T[e]:.2f}$y ({e.isoformat()})")
    ax2.axvline(K_MON, ls="--", color="#B26A1C", lw=1.2)
    ax2.axvline(H_MON, ls="--", color="#C0392B", lw=1.2)
    ymin, ymax = ax2.get_ylim()
    ax2.set_ylim(ymin, ymax + 2.5)                # headroom so K/H labels clear the legend
    ymax = ymax + 2.5
    ax2.text(K_MON, ymax, f" $K$ ({K_MON:.0f}%)", color="#B26A1C",
             fontsize=8, va="top", ha="left")
    ax2.text(H_MON, ymax, f" $H$ ({H_MON:.0f}%)", color="#C0392B",
             fontsize=8, va="top", ha="right")
    ax2.set_xlabel("spot-moneyness (%)", fontsize=9)
    ax2.set_ylabel("implied vol (%)", fontsize=9)
    ax2.set_title("(b) smile slices; strike $K$ and barrier $H$ marked", fontsize=10)
    ax2.tick_params(labelsize=8)
    ax2.grid(True, ls=":", lw=0.5, alpha=0.6)
    ax2.legend(fontsize=7, frameon=False, loc="center left")

    fig.tight_layout()
    fig.savefig(a.out, bbox_inches="tight")
    print(f"wrote {a.out}: {len(expiries)} expiries x {len(moneyness)} moneyness; "
          f"K={K_MON:.1f}% H={H_MON:.1f}% spot-moneyness")


if __name__ == "__main__":
    main()
