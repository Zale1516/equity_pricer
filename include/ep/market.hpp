#pragma once
#include "ep/date.hpp"
#include "ep/json.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace ep {

class PillarCurve {
public:
    std::vector<double> T;                    // pillar times (years, ascending)
    std::vector<double> v;                    // pillar values
    void add(double t, double val) { T.emplace_back(t); v.emplace_back(val); }
    double operator()(double t) const {
        if (T.empty()) return 0.0;
        if (t <= T.front()) return v.front();
        if (t >= T.back())  return v.back();
        auto it = std::lower_bound(T.begin(), T.end(), t);
        size_t hi = it - T.begin(), lo = hi - 1;
        double w = (t - T[lo]) / (T[hi] - T[lo]);
        return v[lo] + w * (v[hi] - v[lo]);
    }
};

class DiscountCurve {
public:
    PillarCurve w;                            // w(T) = z(T) * T = -ln DF(T)
    double df(double T) const { return std::exp(-w(T)); }
    double rate(double T) const { return T > 1e-9 ? w(T) / T : 0.0; }   // zero rate z(T)
};

class ForwardCurve {
public:
    double      spot = 0.0;
    PillarCurve wr;                           // w_r(T) = r_fwd(T) * T  (funding/MMkt time-weighted zero)
    PillarCurve wq;                           // w_q(T) = q(T)   * T    (implied-dividend time-weighted)
    double fwd(double T) const { return T <= 0.0 ? spot : spot * std::exp(wr(T) - wq(T)); }
};

class Market {
public:
    Date          asof;
    double        spot = 0.0;
    DiscountCurve disc;
    ForwardCurve  fwdc;

    double df(double T) const  { return disc.df(T); }
    double fwd(double T) const { return fwdc.fwd(T); }

    static Market from_json(const json::Value& j);                  // build from market.json
    static Date parse_ymd_slash_or_dash(const std::string& s);     // accept "YYYY-MM-DD" or "YYYY/M/D"
};

} // namespace ep
