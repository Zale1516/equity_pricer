# Accumulator Pricing — Reference Notes

Theory reference for this pricer. Primary source: **Lam, Yu & Xin (2009), "Accumulator
Pricing", IEEE CIFEr** (HKU/HKBU). These notes distill the method, organise it along the
axes that matter, classify our deal, and record one correction to the paper.

---

## 1. Core idea: an accumulator is a strip of barrier options

An accumulator obliges the buyer to purchase stock on observation days `t_1..t_n` at a fixed
**discounted strike `K < S_0`**, terminating (knock-out) if the price breaches an **up barrier
`H`**. It is a zero-cost, strongly path-dependent structure.

Payoff on observation day `t_i` (immediate settlement, gearing `g`), given not knocked out:

```
  S_i >= K :   (S_i - K)          # like a long call
  S_i <  K :   g * (S_i - K)      # forced to buy g x  -> like short g puts
  knocked out: 0
```

This equals **long 1 up-and-out call − g up-and-out puts** (strike `K`, barrier `H`, expiry
`t_i`). Summed over all observation days:

```
  V = Σ_i [ C_uo(t_i, K, H) − g · P_uo(t_i, K, H) ]                        (paper eq. 2)
```

The `C_uo`, `P_uo` have Reiner–Rubinstein (1991) closed forms under Black–Scholes.
**Everything else is choosing how to evaluate these barrier options.**

---

## 2. The axes that define an accumulator

### Axis A — Gearing (g)
`g` multiplies the put leg. `g = 2` ("typical", the dangerous *I-kill-you-later*): 2× downside
below `K`. `g = 1` (ungeared): each day's payoff is the unconditional forward `S_i − K` (call −
put), i.e. `C_uo − P_uo`. Risk is highly asymmetric — long left tail (no downside protection),
short right tail (KO caps the upside); the buyer's VaR is several times the seller's.

### Axis B — Barrier monitoring: continuous vs discrete
- **Continuous:** KO if the barrier is touched at any instant → RR closed form (paper eq. 5).
- **Discrete (e.g. daily close):** the continuous formula misprices it. Broadie–Glasserman–Kou
  (1997) correct it with a **shifted barrier**
  ```
    H~ = H · exp( β σ sqrt(T/m) ),   β = −ζ(1/2)/sqrt(2π) ≈ 0.5826,  m = # monitoring points
  ```
  and use `V_discrete(H) ≈ V(H~)` (paper eq. 8). For daily monitoring the shift is small
  (~1%). Alternatively, price **numerically with daily steps** (PDE/tree/MC) — exact for the
  discrete monitoring, no correction needed.

### Axis C — Settlement: immediate vs delay
- **Immediate:** delivered on `t_i` → barrier options on **spot** (eq. 5).
- **Delay** (settled periodically on `T_i`): the `t_i` payoff is the value of a forward to
  `T_i` → barrier options on **forwards** (paper eq. 7). Only the discounting changes.

The three axes are independent — pick `g`, pick `H`/`H~`, pick spot/forward barrier options.

---

## 3. Where our deal (0027.HK, Citi H19871600A) sits

| Axis | Term-sheet fact | Class |
|---|---|---|
| Gearing | "Daily Number of Shares 234" constant; no double-below-strike, no knock-in | **g = 1 (ungeared)** → per day = forward `C_uo − P_uo` |
| Barrier | KO Determination Days = each scheduled trading day; Settlement Price = close | **discrete, daily-close** |
| Settlement | Physical; Settlement Date = valuation date + 2; ~26 fortnightly periods | **delay, physical** |

**Our deal = ungeared, discrete-barrier (daily), delay-settled, physical accumulator.**

Features beyond the paper: **Guaranteed Delivery Period** (min accrual floor to 2024-12-04),
**245-day cap**, and we add **local volatility** (paper is BS-only). Being `g = 1` is why the
engine accrues `(S − K)·234` unconditionally each day.

---

## 4. Engines in this repo, and why

| Engine | Role | Covers |
|---|---|---|
| **CN-PDE** (`pde_engine.hpp`) | **the pricer** | BS + **local vol**, exact discrete barrier, guaranteed period, forward-start |
| Analytic (`analytic_engine.hpp`) | benchmark (tests) | BS only, g configurable, continuous + BGK-discrete; no guarantee/forward-start |

The closed form **cannot be the pricer**: it is BS-constant-vol (no skew — the whole point of
the preferred LV), has no guaranteed-period floor, and treats the discrete barrier only
approximately (BGK). It is kept as an **analytic anchor** in the test suite and for future fast
Greeks / zero-cost calibration.

### Validation status
On a matched BS scenario (valuation = start, no guarantee, σ = 0.30) the two engines agree:

```
  Analytic(continuous) = −47,735
  Analytic(BGK)        = −45,810      <- discrete correction, closest to the exact-discrete PDE
  CN-PDE               = −46,124
```

BGK sits between the continuous closed form and the exact-discrete CN-PDE, exactly as theory
predicts. (An MC engine was used during bring-up as a third check and has since been retired.)

---

## 5. Correction to the paper

The paper's **printed eq. (4) for `P_uo` is incorrect** (it evaluates to `P_uo > vanilla put`,
which is impossible for an up-and-out put). We instead use the standard Reiner–Rubinstein /
Haug barrier building blocks `A, B, C, D` with, for strike `X < H`:

```
  up-and-out call = A − B + C − D      (φ=+1, η=−1)
  up-and-out put  = A − C              (φ=−1, η=−1)
```

verified against a fine-grid brute-force Monte Carlo (single expiry): `P_uo = 1.51` (formula)
vs `1.52` (brute), against the buggy `2.17`. See `analytic_engine.hpp::rr()`.

---

## 6. What the paper also gives us (future use)
- **Zero-cost calibration**: solve `V = 0` for the fair `K` or the implied vol (paper Tables
  II–III) — natural on the analytic engine.
- **Greeks** by differentiating eq. (5)/(7) (paper Appendix III); discrete → substitute `H~`.
- **Risk asymmetry**: buyer Δ/Γ/Vega much larger when losing (low S) than winning (high S).
