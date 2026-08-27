#pragma once
#include "ep/ssvi.hpp"
#include "ep/market.hpp"

namespace ep {

class DupireLocalVol {
public:
    const SSVI*   ssvi = nullptr;      // fitted smile (non-owning)
    const Market* mkt  = nullptr;      // forward curve (non-owning)
    double vol_floor = 0.02;           // 2% floor

    double sigma(double S, double t) const;   // Dupire local vol sigma_loc(S,t) for the PDE VolFn
};

} // namespace ep
