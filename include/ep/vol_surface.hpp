#pragma once
#include "ep/date.hpp"
#include "ep/json.hpp"
#include <map>
#include <string>

namespace ep {

class VolSurface {
public:
    Date   asof;                 // ValuationDate from the source
    double spot = 0.0;           // spot reference from the source
    std::map<double, std::map<double, double>> byT;   // maturity T -> {spot-moneyness % -> vol}

    static VolSurface load_csv(const std::string& path);                              // build from OVDV CSV export
    static VolSurface from_json(const json::Value& j, const Date& asof, double spot);  // build from market_data.json block
    double vol(double K, double T) const;                                             // implied vol at strike K, expiry T
};

} // namespace ep
