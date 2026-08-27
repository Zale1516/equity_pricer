#pragma once
#include "ep/market.hpp"
#include "ep/vol_surface.hpp"
#include <cmath>

namespace ep {

class SSVI {
public:
    double rho = 0.0, eta = 0.0, gamma = 0.0;   // global smile parameters
    PillarCurve theta;                          // ATM total variance theta_T (node (0,0) included)

    double phi(double th) const { return eta * std::pow(th, -gamma); }

    double w(double k, double T) const {                    // total implied variance at k=ln(S/F), maturity T
        double th = theta(T);
        if (th <= 0.0) return 0.0;
        double ph = phi(th);
        double D  = std::sqrt((ph * k + rho) * (ph * k + rho) + (1.0 - rho * rho));
        return 0.5 * th * (1.0 + rho * ph * k + D);
    }
    double dwdk(double k, double T) const {
        double th = theta(T); if (th <= 0.0) return 0.0;
        double ph = phi(th);
        double D  = std::sqrt((ph * k + rho) * (ph * k + rho) + (1.0 - rho * rho));
        return 0.5 * th * ph * (rho + (ph * k + rho) / D);
    }
    double d2wdk2(double k, double T) const {
        double th = theta(T); if (th <= 0.0) return 0.0;
        double ph = phi(th);
        double D  = std::sqrt((ph * k + rho) * (ph * k + rho) + (1.0 - rho * rho));
        return 0.5 * th * ph * ph * (1.0 - rho * rho) / (D * D * D);
    }
    double dwdT(double k, double T) const {                 // central FD in T on the smooth surface
        double h = std::max(1e-4, 1e-3 * T);
        double Tm = std::max(T - h, 1e-8);
        return (w(k, T + h) - w(k, Tm)) / (T + h - Tm);
    }
    double vol(double k, double T) const {                  // BS implied vol from the fit
        return std::sqrt(std::max(w(k, T) / std::max(T, 1e-8), 1e-12));
    }

    class Quote { public: double k, T, w; };

    static SSVI calibrate(const VolSurface& surf, const Market& mkt, double* rmse_out = nullptr,
                          double Tmax = 1e18);   // fit (rho,eta,gamma) over the front window T<=Tmax
};

} // namespace ep
