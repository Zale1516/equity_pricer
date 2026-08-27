# equity-pricer

A small, self-contained C++20 engine that prices an **equity accumulator with a daily up-and-out
knock-out** (Galaxy Entertainment 0027.HK, Citi deal H19871600A).

The contract is Markovian in the single underlying, so a **1-D Crank–Nicolson PDE** prices it
exactly: the daily barrier is a node-aligned reset, the accrual is an additive forward strip, and
Delta/Gamma read straight off the value surface. Black–Scholes and local volatility share one engine.

## The product (term sheet H19871600A)

| field | value |
|---|---|
| Underlying | Galaxy Entertainment 0027.HK (HKD) |
| Accrual strike `K` / barrier `H` | 31.0804 / 37.1825 (daily close) |
| Shares/day `N` / max days | 234 / 245 |
| Start / Final valuation | 2024-11-07 / 2025-11-05 |
| Guaranteed delivery | through 2024-12-04 |
| Initial exchange (anchor) | Citi → Counterparty 23,906.61 HKD |

The buyer accrues 234 shares/day at `K` while alive. A knock-out (close ≥ `H`) stops all future
accrual, except accrual is guaranteed through 2024-12-04. Valued as-of the trade date **2024-11-06**
(spot 34.50).

## Method

- **Engine** — Crank–Nicolson in `x = ln S`; BS (constant σ) and local vol share it (`pde_engine.hpp`).
- **Volatility** — arbitrage-free **SSVI** fit (front window) → **Dupire** local vol (`ssvi.hpp`, `local_vol.hpp`).
- **Validation** — the PDE converges to the exact **Reiner–Rubinstein** closed form in the
  continuous-monitoring limit (`analytic_engine.hpp`); a toy Monte Carlo (`tools/plot_mc_paths.py`)
  independently reproduces the price.
- **Greeks** — Delta/Gamma read off the grid, Vega by bump-and-revalue.

## Pipeline

```
inputs/hkd_discount.csv  ┐  (OIS zero → discounting)
inputs/carry.csv         ├─ build_market_data.py → market_data.json ┐
data/vol_surface.csv     ┘  (OVME rate + div → forward)             ├─→ ep_app (C++)
                            build_trade.py          → trade.json     ┘
```
Python builds the JSON from Bloomberg exports; the C++ engine ingests only JSON.

## Build & run

```bash
python3 tools/build_market_data.py --disc inputs/hkd_discount.csv \
    --carry inputs/carry.csv --vol data/vol_surface.csv --out data/market_data.json
python3 tools/build_trade.py --out data/trade.json
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
./build/ep_app data/market_data.json data/trade.json
```
Needs a C++20 compiler and **Eigen** (`brew install eigen`, for the SSVI fit).

## Result (trade-date 2024-11-06)

| model | buyer PV (HKD) |
|---|---|
| Black–Scholes (vol at strike) | −39,879 |
| Local volatility (SSVI / Dupire) | −45,956 |
| Anchor (initial exchange) | −23,906 |

The buyer holds a short-volatility position with a fat left tail: the knock-out caps the upside while
the downside is unprotected. The gap to the anchor is dealer margin plus a lower dealer vol mark. Full
write-up in `report/accumulator_pricing.pdf`.
