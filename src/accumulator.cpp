#include "ep/accumulator.hpp"
#include <algorithm>

namespace ep {

AccumulatorKO AccumulatorKO::from_json(const json::Value& j) {
    AccumulatorKO p;
    auto D = [](const std::string& s){ return Market::parse_ymd_slash_or_dash(s); };
    p.K            = j.at("forward_price").as_num();
    p.H            = j.at("knock_out_price").as_num();
    p.daily_shares = j.at("daily_shares").as_num();
    p.max_days     = static_cast<int>(j.at("max_scheduled_days").as_num());
    p.start        = D(j.at("start_date").as_str());
    p.final_val    = D(j.at("final_valuation_date").as_str());
    p.guar_end     = D(j.at("guaranteed_delivery_end").as_str());
    if (j.has("valuation_dates")) {
        for (const auto& v : j.at("valuation_dates").as_arr()) p.valuation_dates.emplace_back(D(v.as_str()));
        std::sort(p.valuation_dates.begin(), p.valuation_dates.end());
    }
    if (j.has("settle_lag")) p.settle_lag = static_cast<int>(j.at("settle_lag").as_num());
    return p;
}

Date AccumulatorKO::settlement_for(const Date& accrual) const {
    if (valuation_dates.empty()) return accrual;
    auto it = std::lower_bound(valuation_dates.begin(), valuation_dates.end(), accrual);
    const Date& vdate = (it != valuation_dates.end()) ? *it : valuation_dates.back();
    return add_hk_business_days(vdate, settle_lag);
}

double AccumulatorKO::evaluate(const std::vector<Date>& grid,
                               const std::vector<double>& path,
                               const Market& mkt) const {
    double pv = 0.0;
    int accrued = 0;
    long guar_serial  = guar_end.serial;
    long start_serial = start.serial;

    for (size_t i = 0; i < grid.size(); ++i) {
        if (grid[i].serial < start_serial) continue;
        if (accrued >= max_days) break;

        if (path[i] >= H) {
            if (grid[i].serial <= guar_serial)
                for (size_t j = i; j < grid.size() && grid[j].serial <= guar_serial; ++j) {
                    if (accrued >= max_days) break;
                    pv += accrue_pv(grid[j], path[j], mkt);
                    ++accrued;
                }
            break;
        }

        pv += accrue_pv(grid[i], path[i], mkt);
        ++accrued;
    }
    return pv;
}

double AccumulatorKO::accrue_pv(const Date& d, double S, const Market& mkt) const {
    double T = yearfrac(mkt.asof, settlement_for(d));
    return mkt.df(T) * (S - K) * daily_shares;
}

} // namespace ep
