#!/usr/bin/env python3
"""Delta and Gamma profiles of the accumulator, read off the PDE value surface.

Reads the as-of local-vol value profile W(S) that ep_app dumps to data/greeks_profile.csv, and
plots Delta = dV/dS and Gamma = d2V/dS2 across spot. It is the same finite-difference read the
engine uses at the spot node, applied at every node, so it shows where the risk concentrates.

Run ep_app first to (re)generate the CSV, then: python3 tools/plot_greeks.py
"""
import argparse
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

K, H, SPOT = 31.0804, 37.1825, 34.50


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default="data/greeks_profile.csv")
    ap.add_argument("--out", default="report/fig/greeks.pdf")
    a = ap.parse_args()
    d = np.loadtxt(a.csv, delimiter=",", skiprows=1)
    S, W = d[:, 0], d[:, 1]
    dx = np.log(S[1] / S[0])                       # uniform in x = ln S
    Wx = np.gradient(W, dx)                        # d/dx
    Wxx = np.gradient(Wx, dx)
    delta = Wx / S
    gamma = (Wxx - Wx) / S ** 2

    m = (S >= 22) & (S <= 42)                      # focus on the live region
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax in (a1, a2):
        ax.axvline(K, ls="--", color="#B26A1C", lw=1.1)
        ax.axvline(H, ls="--", color="#C0392B", lw=1.1)
        ax.axvline(SPOT, ls=":", color="gray", lw=1)
        ax.set_xlabel("spot (HKD)"); ax.grid(True, ls=":", lw=0.5, alpha=0.6)
    a1.plot(S[m], delta[m], color="#2E6FB7", lw=1.8)
    a1.set_ylabel("Delta  $\\partial V/\\partial S$"); a1.set_title("(a) Delta", fontsize=11)
    a2.plot(S[m], gamma[m], color="#1E8449", lw=1.8)
    a2.set_yscale("symlog", linthresh=20000)      # spike at H + the smaller at-money level both visible
    a2.set_ylabel("Gamma  $\\partial^2 V/\\partial S^2$  (symlog)"); a2.set_title("(b) Gamma", fontsize=11)
    a1.text(K, a1.get_ylim()[1], " $K$", color="#B26A1C", va="top", fontsize=9)
    a1.text(H, a1.get_ylim()[1], " $H$", color="#C0392B", va="top", fontsize=9)
    a2.text(K, a2.get_ylim()[1], " $K$", color="#B26A1C", va="top", fontsize=9)
    a2.text(H, a2.get_ylim()[1], " $H$", color="#C0392B", va="top", fontsize=9)
    fig.suptitle("Local-vol Greeks read off the PDE value surface (dashed: strike $K$, barrier $H$)", fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    import os; os.makedirs(os.path.dirname(a.out), exist_ok=True)
    fig.savefig(a.out, bbox_inches="tight")
    j = np.argmin(np.abs(S - SPOT))
    print(f"wrote {a.out}  (at spot: Delta={delta[j]:.0f}, Gamma={gamma[j]:.0f})")


if __name__ == "__main__":
    main()
