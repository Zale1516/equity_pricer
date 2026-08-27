# inputs/

Bloomberg data for the trade date **2024-11-06**.

- `hkd_discount.csv` — HKD OIS zero curve (`date, zero_rate`), from ICVS 145. Drives discounting.
- `carry.csv` — forward carry (`date, fwd_rate, div_yield`), from OVME MMkt rate + implied dividend.
  The forward is rebuilt as `F(T) = S·exp((fwd_rate − div_yield)·T)`.
- Vol surface lives in `data/vol_surface.csv` (OVDV grid, spot 34.50).

`tools/build_market_data.py` turns these into `data/market_data.json`.
