#include "ep/ssvi.hpp"
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>
#include <Eigen/Dense>
#include <unsupported/Eigen/NonLinearOptimization>
#include <unsupported/Eigen/NumericalDiff>

namespace ep {

namespace {

double interp_atm(std::vector<std::pair<double,double>>& kw) {
    std::sort(kw.begin(), kw.end());
    for (size_t i = 1; i < kw.size(); ++i)
        if (kw[i].first >= 0.0) {
            double k0 = kw[i-1].first, k1 = kw[i].first;
            double t = (0.0 - k0) / (k1 - k0);
            return kw[i-1].second + t * (kw[i].second - kw[i-1].second);
        }
    return kw.back().second;
}

class SSVIFunctor {
public:
    using Scalar = double;
    using InputType = Eigen::VectorXd;
    using ValueType = Eigen::VectorXd;
    using JacobianType = Eigen::MatrixXd;
    enum { InputsAtCompileTime = Eigen::Dynamic, ValuesAtCompileTime = Eigen::Dynamic };

    const std::vector<SSVI::Quote>* quotes = nullptr;
    const PillarCurve* theta = nullptr;
    std::vector<double> thT, thV;
    double penalty = 50.0;
    int m_values = 0;

    int inputs() const { return 3; }
    int values() const { return m_values; }

    static void unpack(const Eigen::VectorXd& u, double& rho, double& eta, double& gamma) {
        rho   = std::tanh(u[0]);
        eta   = std::exp(u[1]);
        gamma = 0.5 / (1.0 + std::exp(-u[2]));
    }

    int operator()(const Eigen::VectorXd& u, Eigen::VectorXd& fvec) const {
        double rho, eta, gamma; unpack(u, rho, eta, gamma);
        auto phi = [&](double th) { return eta * std::pow(th, -gamma); };
        auto wf  = [&](double k, double th) {
            double ph = phi(th);
            double D  = std::sqrt((ph * k + rho) * (ph * k + rho) + (1.0 - rho * rho));
            return 0.5 * th * (1.0 + rho * ph * k + D);
        };
        int idx = 0;
        for (const auto& q : *quotes) fvec[idx++] = wf(q.k, (*theta)(q.T)) - q.w;
        for (size_t i = 0; i < thT.size(); ++i) {
            double th = thV[i], ph = phi(th);
            fvec[idx++] = penalty * std::max(0.0, th * ph * (1.0 + std::abs(rho)) - 4.0);
            fvec[idx++] = penalty * std::max(0.0, th * ph * ph * (1.0 + std::abs(rho)) - 4.0);
        }
        return 0;
    }
};

} // namespace

SSVI SSVI::calibrate(const VolSurface& surf, const Market& mkt, double* rmse_out, double Tmax) {
    SSVI s;
    std::vector<Quote> quotes;
    s.theta.add(0.0, 0.0);
    double prev = 0.0;
    for (const auto& [T, row] : surf.byT) {
        if (T > Tmax) continue;
        std::vector<std::pair<double,double>> kw;
        for (const auto& [pillar, vol] : row) {
            double K = pillar / 100.0 * surf.spot;
            double k = std::log(K / mkt.fwd(T));
            double w = vol * vol * T;
            kw.emplace_back(k, w);
            quotes.emplace_back(Quote{k, T, w});
        }
        double th = std::max(interp_atm(kw), prev + 1e-8);
        s.theta.add(T, th);
        prev = th;
    }

    SSVIFunctor f;
    f.quotes = &quotes; f.theta = &s.theta;
    for (size_t i = 1; i < s.theta.T.size(); ++i) { f.thT.emplace_back(s.theta.T[i]); f.thV.emplace_back(s.theta.v[i]); }
    f.m_values = static_cast<int>(quotes.size() + 2 * f.thT.size());

    Eigen::NumericalDiff<SSVIFunctor> numDiff(f);
    Eigen::LevenbergMarquardt<Eigen::NumericalDiff<SSVIFunctor>> lm(numDiff);
    Eigen::VectorXd u(3);
    u << std::atanh(-0.3), std::log(1.0), 0.4;
    lm.minimize(u);
    SSVIFunctor::unpack(u, s.rho, s.eta, s.gamma);

    if (rmse_out) {
        double ss = 0.0;
        for (const auto& q : quotes) { double d = s.w(q.k, q.T) - q.w; ss += d * d; }
        *rmse_out = std::sqrt(ss / quotes.size());
    }
    return s;
}

} // namespace ep
