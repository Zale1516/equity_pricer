// Smoke tests: date math, surface query, payoff sanity, analytic-vs-PDE, SSVI/Dupire.
#include "ep/date.hpp"
#include "ep/vol_surface.hpp"
#include "ep/market.hpp"
#include "ep/accumulator.hpp"
#include "ep/pde_engine.hpp"
#include "ep/analytic_engine.hpp"
#include "ep/ssvi.hpp"
#include "ep/local_vol.hpp"
#include <iostream>
#include <cmath>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } \
                              else { std::cout << "ok  : " << (msg) << "\n"; } } while (0)

// Test fixture: the 0027.HK / Citi accumulator (Deal H19871600A) built explicitly.
// AccumulatorKO no longer carries term-sheet defaults; production builds it from trade.json.
static ep::AccumulatorKO test_accumulator() {
    ep::AccumulatorKO p;
    p.K = 31.0804; p.H = 37.1825; p.daily_shares = 234.0; p.max_days = 245;
    p.start     = ep::Date(2024, 11, 7);   // first scheduled trading day: accrual + KO begin here
    p.final_val = ep::Date(2025, 11, 5); p.guar_end = ep::Date(2024, 12, 4);
    return p;
}

int main(int argc, char** argv) {
    using namespace ep;
    std::string csv = (argc > 1) ? argv[1] : "data/vol_surface.csv";

    // --- date math ---
    CHECK(days_from_civil(1970, 1, 1) == 0, "epoch serial == 0");
    CHECK(is_weekend(Date(2024, 11, 9)) && is_weekend(Date(2024, 11, 10)), "2024-11-09/10 are Sat/Sun");
    CHECK(!is_weekend(Date(2024, 11, 7)), "2024-11-07 is a weekday (Thu)");
    CHECK(std::abs(yearfrac(Date(2024, 11, 6), Date(2025, 11, 6)) - 1.0) < 0.01, "1Y yearfrac ~ 1.0");

    // --- CSV parse / surface ---
    VolSurface surf = VolSurface::load_csv(csv);
    CHECK(std::abs(surf.spot - 34.50) < 1e-9, "spot parsed == 34.50");
    CHECK(surf.byT.size() >= 10, "at least 10 expiries parsed");
    double v = surf.vol(31.0804, 1.0);          // ~95% moneyness, ~1Y
    CHECK(v > 0.20 && v < 0.50, "vol(K,1Y) in a sane range (20%-50%)");

    // --- payoff sanity ---
    Market mkt; mkt.asof = surf.asof; mkt.spot = surf.spot;
    AccumulatorKO prod = test_accumulator();
    std::vector<Date> grid = business_days(mkt.asof, prod.final_val);
    // path that never knocks out and sits above K -> positive PV
    std::vector<double> flat(grid.size(), 33.0);
    double pv_up = prod.evaluate(grid, flat, mkt);
    CHECK(pv_up > 0.0, "constant S=33 (>K, <H) gives positive buyer PV");
    // path exactly at K -> ~zero PV
    std::vector<double> atK(grid.size(), prod.K);
    CHECK(std::abs(prod.evaluate(grid, atK, mkt)) < 1e-6, "S==K gives ~zero PV");

    // --- Engine validation: CN-PDE vs closed form, SAME constant vol on both sides ---
    // Matched BS scenario: valuation one business day before the Start Date (a genuine t>0 accrual
    // strip, matching the real trade convention), no Guaranteed Delivery Period, constant sigma.
    AccumulatorKO p3 = prod;
    p3.guar_end = Date(2024, 11, 5);                          // before start -> disable guarantee
    Market m3; m3.asof = Date(2024, 11, 6); m3.spot = 34.50;  // as-of one business day before start
    m3.disc.w.add(0.0, 0.0); m3.disc.w.add(3.0, 0.04 * 3.0);   // flat z = 4%
    m3.fwdc.spot = m3.spot;                                    // F=S*exp(w_r-w_q): r=4%, q=1%
    m3.fwdc.wr.add(0.0, 0.0); m3.fwdc.wr.add(3.0, 0.04 * 3.0);
    m3.fwdc.wq.add(0.0, 0.0); m3.fwdc.wq.add(3.0, 0.01 * 3.0);
    double s3 = 0.391;                                       // trade-regime constant vol (BS vol at strike)
    VolFn cv = [s3](double, double) { return s3; };

    AnalyticAccumulatorEngine an; an.gearing = 1.0;
    AnalyticResult ra = an.run(p3, m3, s3);

    // (a) Continuous-monitoring convergence: absorb the barrier at every intraday sub-step and refine
    //     dt. The PDE must converge to the EXACT continuous Reiner-Rubinstein closed form.
    CrankNicolsonEngine pcont; pcont.substeps = 128; pcont.continuous_barrier = true;
    double cn_cont = pcont.run(p3, m3, surf, cv).price;
    double gap_cont = std::abs(cn_cont - ra.price_continuous) / std::abs(ra.price_continuous);
    // (b) Daily-discrete (the contract): CN exact-discrete vs the approximate BGK closed form.
    CrankNicolsonEngine pday; PdeResult rp3 = pday.run(p3, m3, surf, cv);

    std::cout << "  Continuous: Analytic(RR)=" << ra.price_continuous << "  CN(128 substeps)=" << cn_cont
              << "  gap=" << 100.0 * gap_cont << "%\n";
    std::cout << "  Discrete:   Analytic(BGK)=" << ra.price_bgk << "  CN(daily)=" << rp3.price << "\n";
    CHECK(gap_cont < 0.005, "CN(continuous) -> exact RR closed form within 0.5% (engine validated)");

    // --- SSVI calibration + Dupire local vol ---
    Market ms; ms.asof = surf.asof; ms.spot = surf.spot;
    ms.fwdc.spot = surf.spot;                                      // F=S*exp(w_r-w_q): r-q = 2%
    ms.fwdc.wr.add(0.0, 0.0); ms.fwdc.wr.add(3.0, 0.02 * 3.0);
    ms.fwdc.wq.add(0.0, 0.0); ms.fwdc.wq.add(3.0, 0.0);
    double srmse = 0.0;
    SSVI sv = SSVI::calibrate(surf, ms, &srmse);
    std::cout << "  SSVI: rho=" << sv.rho << " eta=" << sv.eta << " gamma=" << sv.gamma
              << " rmse=" << srmse << "\n";
    CHECK(srmse < 0.02, "SSVI fit RMSE < 0.02 (total variance)");

    DupireLocalVol dlv; dlv.ssvi = &sv; dlv.mkt = &ms;
    double lv_atm = dlv.sigma(surf.spot, 1.0);
    CHECK(lv_atm > 0.15 && lv_atm < 0.55, "Dupire local vol at ATM/1Y in a sane range");
    bool lv_ok = true;
    for (double m : {0.6, 0.8, 1.0, 1.2, 1.4})
        for (double Tt : {0.25, 0.5, 1.0}) {
            double lv = dlv.sigma(m * surf.spot, Tt);
            if (!(lv > 0.05 && lv < 1.2)) lv_ok = false;
        }
    CHECK(lv_ok, "Dupire local vol positive and bounded across (S,T)");

    std::cout << (failures ? "\nSOME TESTS FAILED\n" : "\nALL TESTS PASSED\n");
    return failures ? 1 : 0;
}
