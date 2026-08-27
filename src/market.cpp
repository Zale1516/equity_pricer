#include "ep/market.hpp"

namespace ep {

Market Market::from_json(const json::Value& j) {
    Market m;
    m.asof = parse_ymd_slash_or_dash(j.at("asof").as_str());
    m.spot = j.at("spot").as_num();
    m.fwdc.spot = m.spot;
    m.disc.w.add(0.0, 0.0);
    for (const auto& p : j.at("discount_curve").at("pillars").as_arr()) {
        double T = p.at("T").as_num();
        m.disc.w.add(T, p.at("zero_rate").as_num() * T);
    }
    m.fwdc.wr.add(0.0, 0.0);
    m.fwdc.wq.add(0.0, 0.0);
    for (const auto& p : j.at("forward_curve").at("pillars").as_arr()) {
        double T = p.at("T").as_num();
        m.fwdc.wr.add(T, p.at("fwd_rate").as_num()  * T);
        m.fwdc.wq.add(T, p.at("div_yield").as_num() * T);
    }
    return m;
}

Date Market::parse_ymd_slash_or_dash(const std::string& s) {
    std::string t = s; for (char& c : t) if (c == '-') c = '/';
    return parse_ymd_slash(t);
}

} // namespace ep
