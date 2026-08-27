#include "ep/pde_engine.hpp"
#include "ep/date.hpp"
#include <cmath>
#include <algorithm>

namespace ep {

namespace {

void thomas(std::vector<double>& a, std::vector<double>& b,
            std::vector<double>& c, std::vector<double>& d,
            std::vector<double>& out) {
    size_t n = b.size();
    std::vector<double> cp(n), dp(n);
    cp[0] = c[0] / b[0]; dp[0] = d[0] / b[0];
    for (size_t i = 1; i < n; ++i) {
        double m = b[i] - a[i] * cp[i - 1];
        cp[i] = c[i] / m;
        dp[i] = (d[i] - a[i] * dp[i - 1]) / m;
    }
    out[n - 1] = dp[n - 1];
    for (size_t i = n - 1; i-- > 0; ) out[i] = dp[i] - cp[i] * out[i + 1];
}

} // namespace

PdeResult CrankNicolsonEngine::run(const AccumulatorKO& prod, const Market& mkt,
                                   const VolSurface& surf, const VolFn& sigma) const {
    std::vector<Date> obs = business_days(prod.start, prod.final_val);
    if (obs.size() > static_cast<size_t>(prod.max_days)) obs.resize(prod.max_days);
    std::vector<Date> day;
    day.reserve(obs.size() + 1);
    day.emplace_back(mkt.asof);
    for (const Date& d : obs) if (d.serial > mkt.asof.serial) day.emplace_back(d);
    const size_t Nd = day.size();
    std::vector<double> Td(Nd), Fd(Nd), DFd(Nd);
    for (size_t i = 0; i < Nd; ++i) {
        Td[i]  = yearfrac(mkt.asof, day[i]);
        Fd[i]  = mkt.fwd(Td[i]);
        DFd[i] = mkt.df(yearfrac(mkt.asof, prod.settlement_for(day[i])));
    }

    const int ns = std::max(1, substeps);
    std::vector<double> T;  std::vector<int> obsAt;
    T.reserve((Nd - 1) * ns + 1);  obsAt.reserve((Nd - 1) * ns + 1);
    T.emplace_back(Td[0]);
    obsAt.emplace_back(day[0].serial >= prod.start.serial ? 0 : -1);
    for (size_t i = 1; i < Nd; ++i)
        for (int s = 1; s <= ns; ++s) {
            T.emplace_back(Td[i - 1] + (double)s / ns * (Td[i] - Td[i - 1]));
            obsAt.emplace_back(s == ns ? static_cast<int>(i) : -1);
        }
    const size_t M = T.size();
    std::vector<double> F(M);
    for (size_t k = 0; k < M; ++k) F[k] = mkt.fwd(T[k]);

    double xc = std::log(mkt.spot), xH = std::log(prod.H);
    double Ttot = T.back();
    double sref = surf.vol(prod.K, Ttot);
    int    mB   = intervals_to_barrier;
    double dx   = (xH - xc) / mB;
    double half = width_std * sref * std::sqrt(std::max(Ttot, 1e-6));
    int    n_up = std::max(mB + 10, (int)std::ceil(half / dx));
    int    n_dn = (int)std::ceil(half / dx);
    int    Nx   = n_up + n_dn;
    int    j0   = n_dn;
    std::vector<double> x(Nx + 1), S(Nx + 1);
    for (int j = 0; j <= Nx; ++j) { x[j] = xc + (j - j0) * dx; S[j] = std::exp(x[j]); }

    std::vector<double> W(Nx + 1, 0.0), rhs(Nx + 1), a(Nx + 1), b(Nx + 1), c(Nx + 1);

    auto obs_apply = [&](std::vector<double>& V, int di) {
        bool inGuar = (day[di].serial <= prod.guar_end.serial);
        for (int j = 0; j <= Nx; ++j) {
            if (S[j] >= prod.H) {
                if (inGuar) {
                    double v = 0.0;
                    for (size_t u = di; u < Nd; ++u) {
                        if (day[u].serial > prod.guar_end.serial) break;
                        v += prod.daily_shares * DFd[u] * (S[j] * Fd[u] / Fd[di] - prod.K);
                    }
                    V[j] = v;
                } else V[j] = 0.0;
            } else {
                V[j] += prod.daily_shares * DFd[di] * (S[j] - prod.K);
            }
        }
    };
    auto absorb = [&](std::vector<double>& V) {
        for (int j = 0; j <= Nx; ++j) if (S[j] >= prod.H) V[j] = 0.0;
    };
    auto apply_node = [&](std::vector<double>& V, size_t k) {
        if (obsAt[k] >= 0)                         obs_apply(V, obsAt[k]);
        else if (continuous_barrier && k > 0)      absorb(V);
    };

    apply_node(W, M - 1);

    for (size_t k = M - 1; k-- > 0; ) {
        double dt      = T[k + 1] - T[k];
        double driftrq = std::log(F[k + 1] / F[k]) / dt;
        double A       = 0.5 * dt;
        for (int j = 1; j < Nx; ++j) {
            double sig = sigma(S[j], T[k]);
            double al  = 0.5 * sig * sig;
            double be  = driftrq - 0.5 * sig * sig;
            double Ll  = al / (dx * dx) - be / (2 * dx);
            double Ld  = -2 * al / (dx * dx);
            double Lu  = al / (dx * dx) + be / (2 * dx);
            a[j] = -A * Ll;
            b[j] = 1.0 - A * Ld;
            c[j] = -A * Lu;
            rhs[j] = W[j] + A * (Ll * W[j - 1] + Ld * W[j] + Lu * W[j + 1]);
        }
        {
            double sig = sigma(S[0], T[k]);
            double be  = driftrq - 0.5 * sig * sig;
            double Ld  = -be / dx, Lu = be / dx;
            b[0] = 1.0 - A * Ld; c[0] = -A * Lu; a[0] = 0.0;
            rhs[0] = W[0] + A * (Ld * W[0] + Lu * W[1]);
        }
        {
            double sig = sigma(S[Nx], T[k]);
            double be  = driftrq - 0.5 * sig * sig;
            double Ll  = -be / dx, Ld = be / dx;
            a[Nx] = -A * Ll; b[Nx] = 1.0 - A * Ld; c[Nx] = 0.0;
            rhs[Nx] = W[Nx] + A * (Ll * W[Nx - 1] + Ld * W[Nx]);
        }
        thomas(a, b, c, rhs, W);
        apply_node(W, k);
    }

    PdeResult r;
    r.price = W[j0];
    r.vol_ref = sref; r.nx = Nx + 1; r.nt = M;
    double Wx  = (W[j0 + 1] - W[j0 - 1]) / (2.0 * dx);
    double Wxx = (W[j0 + 1] - 2.0 * W[j0] + W[j0 - 1]) / (dx * dx);
    r.delta = Wx / mkt.spot;
    r.gamma = (Wxx - Wx) / (mkt.spot * mkt.spot);
    r.S_grid = S; r.W_grid = W;
    return r;
}

} // namespace ep
