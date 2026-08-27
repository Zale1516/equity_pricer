#pragma once
#include "ep/market.hpp"
#include "ep/vol_surface.hpp"
#include "ep/accumulator.hpp"
#include <vector>
#include <functional>

namespace ep {

class PdeResult {
public:
    double price = 0.0;    // PV to buyer, as-of
    double vol_ref = 0.0;  // vol used to size the grid
    size_t nx = 0;         // spatial nodes
    size_t nt = 0;         // time layers (business days)
    double delta = 0.0;    // dV/dS at spot, read off the grid
    double gamma = 0.0;    // d2V/dS2 at spot, read off the grid
    std::vector<double> S_grid;  // spot at each node (as-of layer)
    std::vector<double> W_grid;  // value at each node (as-of layer), for the Greeks profile
};

using VolFn = std::function<double(double /*S*/, double /*t_yearfrac*/)>;

class CrankNicolsonEngine {
public:
    int    intervals_to_barrier = 40;    // grid nodes between spot and barrier (barrier-aligned dx)
    double width_std            = 6.0;    // half-domain width in reference std devs
    int    substeps            = 1;       // intraday sub-steps per day (validation: refine dt)
    bool   continuous_barrier  = false;   // absorb barrier every sub-step (validation); default = daily

    PdeResult run(const AccumulatorKO& prod, const Market& mkt,
                  const VolSurface& surf, const VolFn& sigma) const;
};

} // namespace ep
