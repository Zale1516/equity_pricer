// ep_app <market_data.json> <trade.json>  --  Crank-Nicolson accumulator pricer (BS + local vol)
#include "ep/json.hpp"
#include "ep/market.hpp"
#include "ep/vol_surface.hpp"
#include "ep/accumulator.hpp"
#include "ep/pde_engine.hpp"
#include "ep/ssvi.hpp"
#include "ep/local_vol.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    using namespace ep;
    if (argc < 3) { std::cerr << "usage: " << argv[0] << " <market_data.json> <trade.json>\n"; return 2; }
    // --- inputs: market data + trade ---
    json::Value md = json::load(argv[1]);
    json::Value td = json::load(argv[2]);

    Market        mkt  = Market::from_json(md);
    VolSurface    surf = VolSurface::from_json(md.at("vol_surface"), mkt.asof, mkt.spot);
    AccumulatorKO prod = AccumulatorKO::from_json(td);

    double Tref  = yearfrac(mkt.asof, prod.final_val);
    double bsvol = surf.vol(prod.K, Tref);

    VolFn bs_vol = [bsvol](double, double) { return bsvol; };

    double Tcut = 1.5 * Tref;
    double rmse = 0.0;
    SSVI ssvi = SSVI::calibrate(surf, mkt, &rmse, Tcut);
    DupireLocalVol dupire; dupire.ssvi = &ssvi; dupire.mkt = &mkt;
    VolFn lv_vol = [&dupire](double S, double t) { return dupire.sigma(S, t); };

    // --- main pricer: CN-PDE under BS constant vol and Dupire local vol ---
    CrankNicolsonEngine pde;
    PdeResult pde_bs = pde.run(prod, mkt, surf, bs_vol);
    PdeResult pde_lv = pde.run(prod, mkt, surf, lv_vol);

    // --- Greeks: grid Delta/Gamma vs bump-and-revalue benchmark; Vega by vol bump ---
    const double dv = 0.01, hS = 0.0025 * mkt.spot;
    auto bumpS = [&](double s){ Market m = mkt; m.spot = s; m.fwdc.spot = s; return m; };
    VolFn bs_vu=[bsvol,dv](double,double){return bsvol+dv;}, bs_vd=[bsvol,dv](double,double){return bsvol-dv;};
    VolFn lv_vu=[&dupire,dv](double S,double t){return dupire.sigma(S,t)+dv;}, lv_vd=[&dupire,dv](double S,double t){return dupire.sigma(S,t)-dv;};
    double bs_vega=(pde.run(prod,mkt,surf,bs_vu).price - pde.run(prod,mkt,surf,bs_vd).price)/2.0;
    double lv_vega=(pde.run(prod,mkt,surf,lv_vu).price - pde.run(prod,mkt,surf,lv_vd).price)/2.0;
    double bsU=pde.run(prod,bumpS(mkt.spot+hS),surf,bs_vol).price, bsD=pde.run(prod,bumpS(mkt.spot-hS),surf,bs_vol).price;
    double lvU=pde.run(prod,bumpS(mkt.spot+hS),surf,lv_vol).price, lvD=pde.run(prod,bumpS(mkt.spot-hS),surf,lv_vol).price;
    double bs_delta_b=(bsU-bsD)/(2*hS), bs_gamma_b=(bsU-2*pde_bs.price+bsD)/(hS*hS);
    double lv_delta_b=(lvU-lvD)/(2*hS), lv_gamma_b=(lvU-2*pde_lv.price+lvD)/(hS*hS);
    { std::ofstream g("data/greeks_profile.csv"); g<<"S,W\n";
      for(size_t i=0;i<pde_lv.S_grid.size();++i) g<<pde_lv.S_grid[i]<<","<<pde_lv.W_grid[i]<<"\n"; }

    // --- report: PV, Greeks table, trade-date MtM anchor ---
    auto iv = [&](double mon_pct) {
        return ssvi.vol(std::log(mon_pct / 100.0 * mkt.spot / mkt.fwd(1.0)), 1.0);
    };
    double Kmon = prod.K / mkt.spot * 100.0, Hmon = prod.H / mkt.spot * 100.0;

    double anchor = td.has("initial_exchange") ? td.at("initial_exchange").at("amount").as_num() : 0.0;
    std::string tid = td.has("trade_id") ? td.at("trade_id").as_str() : "?";
    std::string und = td.has("underlying") ? td.at("underlying").as_str() : "?";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== equity-pricer :: Accumulator KO (" << und << ", " << tid << ") ===\n";
    std::cout << "asof " << md.at("asof").as_str() << "  spot=" << mkt.spot
              << "  K=" << prod.K << "  H=" << prod.H << "  shares/day=" << prod.daily_shares << "\n";
    std::cout << "r(1Y)=" << std::setprecision(4) << mkt.disc.rate(1.0)
              << "  F(1Y)=" << std::setprecision(2) << mkt.fwd(1.0)
              << "  BSvol(K)=" << std::setprecision(4) << bsvol << "\n";
    std::cout << "engine: CN-PDE  grid " << pde_bs.nx << "x" << pde_bs.nt << "\n";
    std::cout << std::setprecision(4);
    std::cout << "SSVI (front T<=" << std::setprecision(2) << Tcut << "y): rho=" << std::setprecision(4)
              << ssvi.rho << "  eta=" << ssvi.eta << "  gamma=" << ssvi.gamma
              << "  (fit RMSE " << std::setprecision(5) << rmse << " total-var)\n";
    std::cout << std::setprecision(1);
    std::cout << "1Y implied vol @ K(" << Kmon << "%)/ATM/H(" << Hmon << "%): "
              << std::setprecision(3) << iv(Kmon) << "/" << iv(100.0) << "/" << iv(Hmon) << "\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << std::setprecision(2);
    std::cout << "PV buyer  BS : " << std::setw(12) << pde_bs.price << " HKD\n";
    std::cout << "PV buyer  LV : " << std::setw(12) << pde_lv.price
              << " HKD  [front-window SSVI + Dupire local vol]\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << std::setprecision(0);
    std::cout << "Greeks          Delta(grid)  Gamma(grid)   Vega/1vp  | Delta(bump)  Gamma(bump)\n";
    std::cout << "  BS :        " << std::setw(12) << pde_bs.delta << std::setw(13) << pde_bs.gamma
              << std::setw(11) << bs_vega << "  |" << std::setw(12) << bs_delta_b << std::setw(13) << bs_gamma_b << "\n";
    std::cout << "  LV :        " << std::setw(12) << pde_lv.delta << std::setw(13) << pde_lv.gamma
              << std::setw(11) << lv_vega << "  |" << std::setw(12) << lv_delta_b << std::setw(13) << lv_gamma_b << "\n";
    if (anchor != 0.0) {
        std::cout << "-------------------------------------------------------------\n";
        std::cout << "Initial Exchange (Citi->buyer): " << anchor << " HKD\n";
        std::cout << "  => accumulator MtM to buyer ~ " << -anchor << " HKD  [trade-date anchor]\n";
    }
    std::cout << "=============================================================\n";
    return 0;
}
