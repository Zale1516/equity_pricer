#!/usr/bin/env python3
"""Toy Monte-Carlo cross-check of the accumulator, and the figure used in the report.

Simulates daily GBM spot paths under the market's forward drift and a constant Black-Scholes vol,
applies the exact contract (daily KO, guaranteed period, 245-day cap), and reports the buyer PV.
It is an independent check of the CN-PDE engine (BS mode), not a production pricer.

Usage: python3 tools/plot_mc_paths.py [--sigma 0.391] [--out report/fig/mc_paths.pdf]
"""
import argparse, json
from datetime import date, timedelta
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

K, H, N = 31.0804, 37.1825, 234.0
ASOF, FINAL, GUAR_END = date(2024, 11, 6), date(2025, 11, 5), date(2024, 12, 4)
MAX_DAYS = 245
HOLS = {date(2024,12,25),date(2024,12,26),date(2025,1,1),date(2025,1,29),date(2025,1,30),
        date(2025,1,31),date(2025,4,4),date(2025,4,18),date(2025,4,21),date(2025,5,1),
        date(2025,5,5),date(2025,7,1),date(2025,10,1),date(2025,10,7),date(2025,10,29)}


def bdays(s, e):
    out, d = [], s
    while d <= e:
        if d.weekday() < 5 and d not in HOLS:
            out.append(d)
        d += timedelta(days=1)
    return out


def curves(md):
    grid = bdays(ASOF, FINAL)
    T = np.array([(d - ASOF).days / 365.0 for d in grid])
    disc = md["discount_curve"]["pillars"]
    dT = np.array([0.0] + [p["T"] for p in disc]); dW = np.array([0.0] + [p["zero_rate"]*p["T"] for p in disc])
    DF = np.exp(-np.interp(T, dT, dW))
    fwd = md["forward_curve"]["pillars"]
    fT = np.array([0.0] + [p["T"] for p in fwd])
    wr = np.array([0.0] + [p["fwd_rate"]*p["T"] for p in fwd]); wq = np.array([0.0] + [p["div_yield"]*p["T"] for p in fwd])
    F = md["spot"] * np.exp(np.interp(T, fT, wr) - np.interp(T, fT, wq))
    guar_idx = sum(1 for d in grid[1:] if d <= GUAR_END)
    return grid, T, F, DF, guar_idx


def simulate(spot, T, F, DF, guar_idx, sig, Np, seed):
    rng = np.random.default_rng(seed)
    dt = np.diff(T)
    drift = np.log(F[1:] / F[:-1]) - 0.5 * sig * sig * dt
    vol = sig * np.sqrt(dt)
    S = np.exp(np.log(spot) + np.cumsum(drift + vol * rng.standard_normal((Np, len(dt))), axis=1))
    hit = S >= H
    anyko = hit.any(1)
    ko = np.where(anyko, hit.argmax(1) + 1, 10**9)                 # 1-based obs index of first KO
    last = np.full(Np, MAX_DAYS)
    last[ko <= guar_idx] = guar_idx                               # KO within guarantee -> accrue to guar_end
    m2 = (ko > guar_idx) & anyko
    last[m2] = ko[m2] - 1                                         # KO after guarantee -> last accrual is the prior day
    mask = np.arange(1, MAX_DAYS + 1)[None, :] <= last[:, None]
    accr = N * DF[1:][None, :] * (S - K) * mask
    return S, anyko, ko, last, accr.sum(1), np.cumsum(accr, 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--md", default="data/market_data.json")
    ap.add_argument("--sigma", type=float, default=0.391)
    ap.add_argument("--out", default="report/fig/mc_paths.pdf")
    a = ap.parse_args()
    md = json.load(open(a.md)); spot = md["spot"]; sig = a.sigma
    grid, T, F, DF, guar_idx = curves(md)
    Tobs = T[1:]

    pv_big = simulate(spot, T, F, DF, guar_idx, sig, 120000, 1)[4]
    S, anyko, ko, last, pv, cum = simulate(spot, T, F, DF, guar_idx, sig, 400, 7)

    fig = plt.figure(figsize=(13, 8))
    ax = fig.add_subplot(2, 2, (1, 3))
    xf = np.concatenate([[0.0], Tobs])
    for i in range(400):
        L = last[i]
        c = '#2E6FB7' if anyko[i] else '#C0392B'
        ax.plot(xf[:L + 1], np.concatenate([[spot], S[i, :L]]), color=c, lw=0.5, alpha=0.35)
        if anyko[i] and ko[i] <= MAX_DAYS:
            ax.plot(Tobs[ko[i] - 1], S[i, ko[i] - 1], '.', color='#1B4F86', ms=3, alpha=0.6)
    ax.axhline(H, color='k', ls='--', lw=1.3); ax.text(0.02, H + 0.3, f'barrier $H$={H}', fontsize=9)
    ax.axhline(K, color='#1E8449', ls='--', lw=1.3); ax.text(0.02, K - 0.9, f'strike $K$={K}', fontsize=9, color='#1E8449')
    ax.axhline(spot, color='gray', ls=':', lw=1); ax.text(0.02, spot + 0.25, f'$S_0$={spot}', fontsize=8, color='gray')
    ax.set_xlabel('time (yr)'); ax.set_ylabel('spot (HKD)'); ax.set_ylim(20, 46); ax.set_xlim(0, 1.02)
    ax.set_title('(a) 400 simulated spot paths  ---  blue = knocked out, red = survived to final', fontsize=10)
    ax.legend(handles=[Line2D([], [], color='#2E6FB7', label=f'KO ({anyko.mean()*100:.0f}%)'),
                       Line2D([], [], color='#C0392B', label=f'survived ({(1-anyko.mean())*100:.0f}%)')],
              fontsize=8, loc='lower left')

    ax2 = fig.add_subplot(2, 2, 2)
    ax2.hist(pv_big, bins=120, range=(-250000, 120000), color='#7f8fa6', alpha=0.85)
    ax2.axvline(pv_big.mean(), color='#C0392B', lw=1.6, label=f'mean {pv_big.mean():,.0f}')
    ax2.axvline(np.median(pv_big), color='#1E8449', lw=1.6, label=f'median {np.median(pv_big):,.0f}')
    ax2.axvline(-23907, color='#B26A1C', lw=1.6, ls='--', label='anchor -23,907')
    ax2.axvline(0, color='k', lw=0.8)
    ax2.set_xlabel('per-path PV to buyer (HKD)'); ax2.set_ylabel('paths')
    ax2.set_title(f'(b) PV distribution:  {(pv_big>=0).mean()*100:.0f}% positive, fat left tail  '
                  f'(P[PV<0]={(pv_big<0).mean()*100:.0f}%)', fontsize=9)
    ax2.legend(fontsize=8)

    ax3 = fig.add_subplot(2, 2, 4)
    order = np.argsort(pv)
    picks = {'big loser': order[0], 'median': order[len(order)//2],
             'KO winner': int(ko.argmin()), 'big winner': order[-1]}
    col = {'big loser': '#C0392B', 'median': 'gray', 'KO winner': '#2E6FB7', 'big winner': '#1E8449'}
    for name, i in picks.items():
        L = last[i]
        ax3.plot(Tobs[:L], cum[i, :L], color=col[name], lw=1.6, label=f'{name} (PV {pv[i]:,.0f})')
        if anyko[i] and ko[i] <= L:
            ax3.plot(Tobs[ko[i] - 1], cum[i, ko[i] - 1], 'o', color=col[name], ms=5)
    ax3.axhline(0, color='k', lw=0.6)
    ax3.set_xlabel('time (yr)'); ax3.set_ylabel('cumulative PV (HKD)')
    ax3.set_title('(c) cumulative accrual PV, sample paths (dot = KO)', fontsize=9)
    ax3.legend(fontsize=7, loc='lower left')

    fig.suptitle(f'Toy Monte-Carlo of the 0027.HK accumulator '
                 f'(BS $\\sigma$={sig}, {len(pv_big):,} paths, MtM buyer = {pv_big.mean():,.0f} HKD)',
                 fontsize=11, y=0.98)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    import os; os.makedirs(os.path.dirname(a.out), exist_ok=True)
    fig.savefig(a.out, bbox_inches='tight')
    print(f"wrote {a.out}: MtM={pv_big.mean():.0f}  P(KO)={anyko.mean():.3f}  median={np.median(pv_big):.0f}")


if __name__ == "__main__":
    main()
