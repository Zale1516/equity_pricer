#pragma once
#include "ep/date.hpp"
#include "ep/market.hpp"
#include "ep/json.hpp"
#include <vector>

namespace ep {

class AccumulatorKO {
public:
    double K = 0.0;             // Forward Price (accrual strike), HKD
    double H = 0.0;             // Knock-out Price, HKD
    double daily_shares = 0.0;  // Daily Number of Shares
    int    max_days = 0;        // Max Number of Scheduled Trading Days
    Date   start;               // Start Date (first scheduled trading day: accrual + KO begin here)
    Date   final_val;           // Final Valuation Date
    Date   guar_end;            // end of Guaranteed Delivery Period
    std::vector<Date> valuation_dates;   // fortnightly accumulation-period ends (sorted)
    int    settle_lag = 2;               // clearance: settlement = valuation date + T+settle_lag HK business days

    static AccumulatorKO from_json(const json::Value& j);           // build from trade.json
    Date settlement_for(const Date& accrual) const;                 // Settlement Date for an accrual day
    double evaluate(const std::vector<Date>& grid,                  // buyer PV of one daily-close path, discounted to asof
                    const std::vector<double>& path,
                    const Market& mkt) const;

private:
    double accrue_pv(const Date& d, double S, const Market& mkt) const;   // one day's N*(S-K) discounted to settlement
};

} // namespace ep
