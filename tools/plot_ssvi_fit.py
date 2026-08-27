#!/usr/bin/env python3
"""Calibrated SSVI smile vs the raw market surface (the figure used in the report).

Plots the arbitrage-free SSVI fit (smooth lines) against the raw OVDV market points (dots) at a few
front-window expiries. The three global parameters (rho, eta, gamma) come from the C++ front-window
calibration; the ATM term structure theta_T is read from the same raw surface, exactly as the engine
does. This reproduces what the pricer feeds into Dupire, so the plot matches the engine.

Usage: python3 tools/plot_ssvi_fit.py [--out report/fig/ssvi_fit.pdf]
"""
import argparse, csv, json
from datetime import date
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import cm

# C++ front-window fit output (SSVI::calibrate, Tcut = 1.5 yr):
RHO, ETA, GAMMA = 0.1487, 0.6732, 0.5000
TCUT = 1.5


def parse_date(s):
    y, m, d = (int(x) for x in s.strip().replace("-", "/").split("/"))
    return date(y, m, d)


def load_surface(path):
    """raw EQ_VS csv -> asof, spot, {T: {moneyness: vol}}."""
    asof = spot = None
    byT = {}
    for c in csv.reader(open(path)):
        if not c or c[0] != "EQ_VS":
            continue
        if asof is None:
            asof, spot = parse_date(c[1]), float(c[14])
        T = (parse_date(c[18]) - asof).days / 365.0
        byT.setdefault(T, {})[float(c[21])] = float(c[23]) / 100.0
    return asof, spot, byT


def ssvi_w(k, theta):
    phi = ETA * theta ** (-GAMMA)
    D = np.sqrt((phi * k + RHO) ** 2 + (1 - RHO ** 2))
    return 0.5 * theta * (1 + RHO * phi * k + D)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vol", default="data/vol_surface.csv")
    ap.add_argument("--md", default="data/market_data.json")
    ap.add_argument("--out", default="report/fig/ssvi_fit.pdf")
    a = ap.parse_args()
    _, spot, byT = load_surface(a.vol)
    md = json.load(open(a.md))
    fwd = md["forward_curve"]["pillars"]
    fT = np.array([0.0] + [p["T"] for p in fwd])
    wr = np.array([0.0] + [p["fwd_rate"] * p["T"] for p in fwd])
    wq = np.array([0.0] + [p["div_yield"] * p["T"] for p in fwd])
    F = lambda T: spot * np.exp(np.interp(T, fT, wr) - np.interp(T, fT, wq))

    # theta_T from each expiry's ATM slice (interp to k=0), kept non-decreasing -- as the engine does
    theta = {}
    prev = 0.0
    for T in sorted(byT):
        if T > TCUT:
            continue
        Ft = F(T)
        ks = np.array([np.log(m / 100.0 * spot / Ft) for m in sorted(byT[T])])
        ws = np.array([byT[T][m] ** 2 * T for m in sorted(byT[T])])
        idx = np.argsort(ks); ks, ws = ks[idx], ws[idx]
        th = np.interp(0.0, ks, ws)
        theta[T] = max(th, prev + 1e-8); prev = theta[T]

    # pick four front expiries to display
    Ts = sorted(theta)
    picks = [Ts[0], Ts[min(3, len(Ts)-1)], Ts[min(5, len(Ts)-1)], Ts[-1]]
    picks = sorted(set(picks))
    colors = cm.viridis(np.linspace(0.05, 0.8, len(picks)))

    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    mgrid = np.linspace(78, 122, 120)
    for T, c in zip(picks, colors):
        Ft = F(T)
        mm = sorted(byT[T]); vv = [byT[T][m] * 100 for m in mm]
        ax.plot(mm, vv, 'o', color=c, ms=5, zorder=3)                     # raw market points
        k = np.log(mgrid / 100.0 * spot / Ft)
        ssvi_vol = np.sqrt(ssvi_w(k, theta[T]) / T) * 100
        ax.plot(mgrid, ssvi_vol, '-', color=c, lw=1.8,
                label=f'$T={T:.2f}$y')                                    # smooth SSVI fit
    Kmon, Hmon = 31.0804 / spot * 100, 37.1825 / spot * 100
    ax.axvline(Kmon, ls='--', color='#B26A1C', lw=1.1); ax.text(Kmon, ax.get_ylim()[1], f' $K$', color='#B26A1C', va='top', fontsize=9)
    ax.axvline(Hmon, ls='--', color='#C0392B', lw=1.1); ax.text(Hmon, ax.get_ylim()[1], f' $H$', color='#C0392B', va='top', fontsize=9)
    ax.set_xlabel('spot-moneyness (%)'); ax.set_ylabel('implied vol (%)')
    ax.set_title('Calibrated SSVI (lines) vs raw market surface (dots)', fontsize=11)
    from matplotlib.lines import Line2D
    h = ax.get_legend_handles_labels()[0]
    h += [Line2D([], [], marker='o', ls='', color='gray', label='market'),
          Line2D([], [], color='gray', label='SSVI fit')]
    ax.legend(handles=h, fontsize=8, ncol=2, frameon=False)
    ax.grid(True, ls=':', lw=0.5, alpha=0.6)
    import os; os.makedirs(os.path.dirname(a.out), exist_ok=True)
    fig.tight_layout(); fig.savefig(a.out, bbox_inches='tight')
    print(f"wrote {a.out}  (rho={RHO} eta={ETA} gamma={GAMMA}; expiries {', '.join(f'{t:.2f}' for t in picks)})")


if __name__ == "__main__":
    main()
