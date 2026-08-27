#include "ep/vol_surface.hpp"
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace ep {

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string cell; std::stringstream ss(line);
    while (std::getline(ss, cell, ',')) out.emplace_back(cell);
    return out;
}

template <class Map>
std::tuple<double, double, double> bracket_keys(const Map& m, double x) {
    auto it = m.lower_bound(x);
    if (it == m.begin())          return {it->first, it->first, 0.0};
    if (it == m.end())            { auto last = std::prev(m.end()); return {last->first, last->first, 0.0}; }
    double hi = it->first; double lo = std::prev(it)->first;
    double w  = (x - lo) / (hi - lo);
    return {lo, hi, w};
}

double interp_pillar(const std::map<double, double>& row, double pillar) {
    auto [p0, p1, w] = bracket_keys(row, pillar);
    double v0 = row.at(p0); if (p0 == p1) return v0;
    return v0 + w * (row.at(p1) - v0);
}

} // namespace

VolSurface VolSurface::load_csv(const std::string& path) {
    VolSurface s;
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open vol csv: " + path);
    std::string line;
    std::getline(f, line);
    bool have_asof = false;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto col = split(line);
        if (col.size() < 24) continue;
        if (!have_asof) { s.asof = parse_ymd_slash(col[1]); s.spot = std::stod(col[14]); have_asof = true; }
        Date   expiry = parse_ymd_slash(col[18]);
        double pillar = std::stod(col[21]);
        double vol    = std::stod(col[23]);
        double T      = yearfrac(s.asof, expiry);
        s.byT[T][pillar] = vol / 100.0;
    }
    if (s.byT.empty()) throw std::runtime_error("no rows parsed from " + path);
    return s;
}

VolSurface VolSurface::from_json(const json::Value& j, const Date& asof, double spot) {
    VolSurface s; s.asof = asof; s.spot = spot;
    if (j.has("spot_ref")) s.spot = j.at("spot_ref").as_num();
    for (const auto& e : j.at("expiries").as_arr()) {
        double T = e.at("T").as_num();
        for (const auto& pt : e.at("points").as_arr())
            s.byT[T][pt.at("pillar").as_num()] = pt.at("vol").as_num();
    }
    if (s.byT.empty()) throw std::runtime_error("vol_surface json has no points");
    return s;
}

double VolSurface::vol(double K, double T) const {
    double pillar = 100.0 * K / spot;
    auto [t0, t1, wt] = bracket_keys(byT, T);
    double s0 = interp_pillar(byT.at(t0), pillar);
    if (t0 == t1) return s0;
    double s1 = interp_pillar(byT.at(t1), pillar);
    double w0 = s0 * s0 * t0, w1 = s1 * s1 * t1;
    double w  = w0 + wt * (w1 - w0);
    double Tq = std::max(T, 1e-8);
    return std::sqrt(std::max(w / Tq, 1e-12));
}

} // namespace ep
