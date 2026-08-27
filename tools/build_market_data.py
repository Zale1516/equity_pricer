#!/usr/bin/env python3
"""Build market_data.json for the equity-pricer C++ engine.

Inputs (all as-of the same date):
  --disc   HKD OIS discount curve CSV (cols: date, zero_rate)   [BBG: ICVS 145 HKD OIS] -> discounting
  --carry  0027.HK carry CSV          (cols: date, fwd_rate, div_yield) [BBG: OVME MMkt rate + Impl Div]
  --vol    EQ_VS implied-vol CSV      (the 波动率数据.csv file)  [given]
  --out    output json path

The forward is NOT interpolated directly. It is rebuilt from a two-curve carry:
  F(T) = S * exp( (r_fwd(T) - q(T)) T ),
with the time-weighted funding zero r_fwd(T)*T and time-weighted implied dividend q(T)*T each
interpolated piecewise-linearly in T (done in the C++ ForwardCurve; here we just pass the pillars
and record the reconstructed F for reference). r_fwd is OVME's MMkt (HIBOR) funding rate, which
reproduces the market (put-call-parity) forward; discounting uses the separate OIS curve.

Usage:
  python3 tools/build_market_data.py --disc inputs/hkd_discount.csv \
      --carry inputs/carry.csv --vol data/vol_surface.csv --out data/market_data.json
"""
import argparse, csv, json, math
from datetime import date


def parse_date(s):
    s = s.strip().replace("/", "-")
    y, m, d = (int(x) for x in s.split("-"))
    return date(y, m, d)


def yearfrac(d0, d1):
    return (d1 - d0).days / 365.0


def read_two_col(path, val_name):
    rows = []
    with open(path, newline="") as f:
        r = csv.DictReader(f)
        for row in r:
            rows.append((parse_date(row["date"]), float(row[val_name])))
    rows.sort()
    return rows


def read_carry(path):
    """Carry CSV -> sorted list of (date, fwd_rate, div_yield)."""
    rows = []
    with open(path, newline="") as f:
        r = csv.DictReader(f)
        for row in r:
            rows.append((parse_date(row["date"]), float(row["fwd_rate"]), float(row["div_yield"])))
    rows.sort()
    return rows


def interp(pillars, t):
    """pillars: sorted list of (T, value); piecewise-linear, flat-extrapolated."""
    if t <= pillars[0][0]:
        return pillars[0][1]
    if t >= pillars[-1][0]:
        return pillars[-1][1]
    for i in range(1, len(pillars)):
        if t <= pillars[i][0]:
            (t0, v0), (t1, v1) = pillars[i - 1], pillars[i]
            w = (t - t0) / (t1 - t0)
            return v0 + w * (v1 - v0)
    return pillars[-1][1]


def load_vol_csv(path):
    """Returns (asof, spot, expiries) where expiries is a dict expiry_date -> list[(pillar,strike,vol)]."""
    asof = None
    spot = None
    exps = {}
    with open(path, newline="") as f:
        r = csv.reader(f)
        next(r)  # header
        for c in r:
            if len(c) < 24 or not c[0]:
                continue
            if asof is None:
                asof = parse_date(c[1])
                spot = float(c[14])
            exp = parse_date(c[18])
            pillar = float(c[21])
            strike = float(c[22])
            vol = float(c[23]) / 100.0
            exps.setdefault(exp, []).append((pillar, strike, vol))
    return asof, spot, exps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--disc", required=True)
    ap.add_argument("--carry", required=True)
    ap.add_argument("--vol", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--asof", help="override valuation date YYYY-MM-DD (re-bases all T)")
    ap.add_argument("--spot", type=float, help="override spot (F=S*exp((r-q)T) auto-scales; re-centres surface)")
    a = ap.parse_args()

    asof, spot, exps = load_vol_csv(a.vol)
    if a.spot:                                  # bump spot: forward auto-scales via F=S*exp(carry)
        spot = a.spot
    if a.asof:                                  # re-base valuation date
        asof = parse_date(a.asof)

    # discount curve (OIS) -> discounting only
    disc_rows = read_two_col(a.disc, "zero_rate")
    disc_pillars = [(yearfrac(asof, d), z) for d, z in disc_rows]

    discount = {
        "day_count": "ACT/365F",
        "comp": "continuous",
        "pillars": [
            {"date": d.isoformat(), "T": round(yearfrac(asof, d), 6), "zero_rate": z}
            for d, z in disc_rows if yearfrac(asof, d) > 0
        ],
    }

    # forward curve: two-curve carry F(T)=S*exp((r_fwd-q)T); rebuilt in C++ from the pillars below.
    carry_rows = read_carry(a.carry)
    fwd_pillars = []
    for d, r_fwd, q in carry_rows:
        T = yearfrac(asof, d)
        if T <= 0:
            continue
        F = spot * math.exp((r_fwd - q) * T)    # reference value (C++ recomputes the same way)
        fwd_pillars.append(
            {"date": d.isoformat(), "T": round(T, 6), "fwd_rate": r_fwd,
             "div_yield": q, "forward": round(F, 6)}
        )
    forward = {
        "spot": spot,
        "construction": "F(T)=S*exp((fwd_rate-div_yield)*T); fwd_rate=OVME MMkt, div_yield=OVME Impl Div; "
                        "time-weighted linear interp of fwd_rate*T and div_yield*T",
        "pillars": fwd_pillars,
    }

    # vol surface
    vs_exps = []
    for exp in sorted(exps.keys()):
        if yearfrac(asof, exp) <= 0:
            continue                            # drop expiries at/before the as-of
        pts = [
            {"pillar": p, "strike": k, "vol": v}
            for (p, k, v) in sorted(exps[exp])
        ]
        vs_exps.append(
            {"date": exp.isoformat(), "T": round(yearfrac(asof, exp), 6), "points": pts}
        )
    vol_surface = {
        "strike_type": "spot_moneyness_pct",
        "spot_ref": spot,
        "expiries": vs_exps,
    }

    out = {
        "asof": asof.isoformat(),
        "currency": "HKD",
        "underlying": "0027.HK",
        "spot": spot,
        "discount_curve": discount,
        "forward_curve": forward,
        "vol_surface": vol_surface,
    }
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)
    print(f"wrote {a.out}: asof={asof} spot={spot} "
          f"disc_pillars={len(disc_pillars)} fwd_pillars={len(fwd_pillars)} "
          f"expiries={len(vs_exps)}")


if __name__ == "__main__":
    main()
