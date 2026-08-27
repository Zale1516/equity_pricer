#include "ep/analytic_engine.hpp"
#include "ep/date.hpp"
#include <vector>
#include <cmath>

namespace ep {

namespace {

double N(double z) { return 0.5 * std::erfc(-z * 0.7071067811865476); }

class RRTerms { public: double A, B, C, D; };

RRTerms rr(double S, double X, double H, double t, double sig,
           double rq, double df, double phi, double eta) {
    double sq   = sig * std::sqrt(t);
    double muH  = (rq - 0.5 * sig * sig) / (sig * sig);
    double x1   = std::log(S / X) / sq + (1.0 + muH) * sq;
    double x2   = std::log(S / H) / sq + (1.0 + muH) * sq;
    double y1   = std::log(H * H / (S * X)) / sq + (1.0 + muH) * sq;
    double y2   = std::log(H / S) / sq + (1.0 + muH) * sq;
    double eqt  = df * std::exp(rq * t);
    double ert  = df;
    double pH   = std::pow(H / S, 2.0 * (muH + 1.0));
    double pH2  = std::pow(H / S, 2.0 * muH);
    RRTerms r;
    r.A = phi*S*eqt*N(phi*x1) - phi*X*ert*N(phi*x1 - phi*sq);
    r.B = phi*S*eqt*N(phi*x2) - phi*X*ert*N(phi*x2 - phi*sq);
    r.C = phi*S*eqt*pH*N(eta*y1) - phi*X*ert*pH2*N(eta*y1 - eta*sq);
    r.D = phi*S*eqt*pH*N(eta*y2) - phi*X*ert*pH2*N(eta*y2 - eta*sq);
    return r;
}

} // namespace

AnalyticResult AnalyticAccumulatorEngine::run(const AccumulatorKO& prod, const Market& mkt, double sigma) const {
    std::vector<Date> g = business_days(mkt.asof, prod.final_val);
    double S = mkt.spot;
    AnalyticResult r;
    int m = 0;
    for (size_t i = 0; i < g.size(); ++i) {
        if (g[i].serial < prod.start.serial) continue;
        ++m;
        if (m > prod.max_days) break;
        double t  = yearfrac(mkt.asof, g[i]);
        if (t <= 0) continue;
        double rq = std::log(mkt.fwd(t) / S) / t;
        double df = mkt.df(t);
        r.price_continuous += prod.daily_shares * element(S, prod.K, prod.H, t, sigma, rq, df);
        double Htil = prod.H * std::exp(BGK_BETA * sigma * std::sqrt(t / m));
        r.price_bgk        += prod.daily_shares * element(S, prod.K, Htil, t, sigma, rq, df);
        r.n_obs = static_cast<size_t>(m);
    }
    return r;
}

double AnalyticAccumulatorEngine::element(double S, double K, double H, double t,
                                          double sig, double rq, double df) const {
    RRTerms c = rr(S, K, H, t, sig, rq, df, +1.0, -1.0);
    RRTerms p = rr(S, K, H, t, sig, rq, df, -1.0, -1.0);
    double Cuo = c.A - c.B + c.C - c.D;
    double Puo = p.A - p.C;
    return Cuo - gearing * Puo;
}

} // namespace ep
