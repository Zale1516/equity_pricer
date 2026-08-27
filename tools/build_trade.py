#!/usr/bin/env python3
"""Build trade.json for the 0027.HK / Citi accumulator (Deal H19871600A).

Parameters are transcribed from the term sheet. Edit the CONFIG block to price a different
accumulator. Usage:
  python3 tools/build_trade.py --out data/trade.json
"""
import argparse, json

CONFIG = {
    "trade_id": "H19871600A",
    "deal_no": "190599763",
    "product": "accumulator_ko",
    "underlying": "0027.HK",
    "underlying_name": "Galaxy Entertainment Group",
    "currency": "HKD",
    "buyer": "Everbright Securities (HK) [Counterparty]",
    "seller": "Citigroup Global Markets Ltd",
    "trade_date": "2024-11-06",
    "start_date": "2024-11-07",      # first scheduled trading day: accrual + KO begin here
    "final_valuation_date": "2025-11-05",
    "forward_price": 31.0804,          # accrual strike K
    "knock_out_price": 37.1825,        # daily up-and-out barrier H (close)
    "ko_barrier_type": "up_and_out_daily_close",
    "daily_shares": 234.0,
    "max_scheduled_days": 245,
    "guaranteed_delivery_end": "2024-12-04",
    "settlement": "physical",
    "max_notional": 1781839.33,
    "valuation_dates": [
        "2024-11-20", "2024-12-04", "2024-12-18", "2025-01-02", "2025-01-15",
        "2025-02-03", "2025-02-12", "2025-02-26", "2025-03-12", "2025-03-26",
        "2025-04-09", "2025-04-23", "2025-05-07", "2025-05-21", "2025-06-04",
        "2025-06-18", "2025-07-02", "2025-07-16", "2025-07-30", "2025-08-13",
        "2025-08-27", "2025-09-10", "2025-09-24", "2025-10-08", "2025-10-22",
        "2025-11-05",
    ],
    "initial_exchange": {
        "payer": "Citigroup",
        "receiver": "Counterparty",
        "amount": 23906.61,
        "date": "2024-11-08",
    },
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    with open(a.out, "w") as f:
        json.dump(CONFIG, f, indent=2)
    print(f"wrote {a.out}: {CONFIG['trade_id']} {CONFIG['underlying']} "
          f"K={CONFIG['forward_price']} H={CONFIG['knock_out_price']} "
          f"vdates={len(CONFIG['valuation_dates'])}")


if __name__ == "__main__":
    main()
