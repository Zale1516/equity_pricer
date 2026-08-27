#pragma once
#include "ep/market.hpp"
#include "ep/accumulator.hpp"

namespace ep {

class AnalyticResult {
public:
    double price_continuous = 0.0;   // continuous-barrier closed form (accumulated)
    double price_bgk        = 0.0;   // discrete-barrier via BGK shift (accumulated)
    size_t n_obs            = 0;
};

class AnalyticAccumulatorEngine {
public:
    double gearing = 1.0;            // g: put-leg multiplier (our deal is g = 1)
    static constexpr double BGK_BETA = 0.5826;

    AnalyticResult run(const AccumulatorKO& prod, const Market& mkt, double sigma) const;

private:
    double element(double S, double K, double H, double t, double sig, double rq, double df) const;
};

} // namespace ep
