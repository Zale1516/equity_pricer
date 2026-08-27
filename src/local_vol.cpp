#include "ep/local_vol.hpp"
#include <cmath>

namespace ep {

double DupireLocalVol::sigma(double S, double t) const {
    double T = std::max(t, 1e-4);
    double k = std::log(S / mkt->fwd(T));
    double w = ssvi->w(k, T);
    if (w <= 1e-10) return vol_floor;
    double wk  = ssvi->dwdk(k, T);
    double wkk = ssvi->d2wdk2(k, T);
    double wT  = ssvi->dwdT(k, T);
    double denom = 1.0 - (k / w) * wk
                 + 0.25 * (-0.25 - 1.0 / w + k * k / (w * w)) * wk * wk
                 + 0.5 * wkk;
    double vloc = (denom > 1e-8) ? wT / denom : 0.0;
    double fl = vol_floor * vol_floor;
    return std::sqrt(vloc > fl ? vloc : fl);
}

} // namespace ep
