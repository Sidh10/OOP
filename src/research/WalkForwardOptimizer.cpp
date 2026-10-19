#include "quant/research/WalkForwardOptimizer.hpp"

#include <algorithm>

#include "quant/core/Errors.hpp"
#include "quant/engine/Backtester.hpp"
#include "quant/research/ParameterGrid.hpp"
#include "quant/strategies/StrategyFactory.hpp"

namespace quant {

WalkForwardOptimizer::WalkForwardOptimizer(WalkForwardConfig cfg, std::shared_ptr<const CostModel> cost)
    : cfg_(cfg), cost_(std::move(cost)) {
    if (!cost_) throw ConfigError("WalkForwardOptimizer needs a cost model");
    if (cfg_.trainBars == 0 || cfg_.testBars == 0 || cfg_.stepBars == 0)
        throw ConfigError("walk-forward window sizes must be > 0");
}

std::vector<WfWindow> WalkForwardOptimizer::makeWindows(size_t n, const WalkForwardConfig& cfg) {
    std::vector<WfWindow> out;
    for (size_t s = 0; s + cfg.trainBars + cfg.testBars <= n; s += cfg.stepBars)
        out.push_back({s, s + cfg.trainBars, s + cfg.trainBars, s + cfg.trainBars + cfg.testBars});
    return out;
}

namespace {
int countWins(const BacktestResult& r) {
    int w = 0;
    for (const auto& t : r.trades)
        if (t.pnl > 0) ++w;
    return w;
}
}  // namespace

WalkForwardReport WalkForwardOptimizer::run(const std::string& strategyName,
                                            const std::vector<CandleSeries>& data) const {
    if (data.empty()) throw DataError("walk-forward: no data");
    WalkForwardReport rep;
    rep.strategy = strategyName;
    for (size_t k = 0; k < data.size(); ++k) rep.universe += (k ? "/" : "") + data[k].symbol();

    const std::unique_ptr<Strategy> proto = StrategyFactory::create(strategyName);
    const std::vector<ParamSet> combos = StrategyFactory::grid(strategyName).combinations();
    if (combos.empty()) throw ConfigError("strategy '" + strategyName + "' has no parameter grid");

    int oosTrades = 0, oosWins = 0;
    double exposureSum = 0, turnoverSum = 0, isSharpeSum = 0;

    for (const WfWindow& w : makeWindows(data[0].size(), cfg_)) {
        WfWindowResult wr;
        wr.window = w;

        // 1) grid search on the train window only
        double bestSharpe = 0;
        bool have = false;
        for (const ParamSet& p : combos) {
            Backtester bt(proto->clone(), cost_, cfg_.initialEquity);
            bt.strategy().setParams(p);
            bt.strategy().train(MarketView(data, w.trainStart, w.trainEnd - 1));
            const BacktestResult tr = bt.run(data, w.trainStart, w.trainEnd);
            if (tr.metrics.numTrades() < cfg_.minTradesTrain) continue;
            if (!have || tr.metrics.sharpe() > bestSharpe) {
                have = true;
                bestSharpe = tr.metrics.sharpe();
                wr.chosen = p;
            }
        }
        if (!have) {
            rep.windows.push_back(wr);  // skipped: nothing qualified
            continue;
        }

        // 2) run the chosen parameters on the unseen test window: retrain on the train window,
        //    start flat with the same initial equity, warm up on earlier (past) bars.
        Backtester bt(proto->clone(), cost_, cfg_.initialEquity);
        bt.strategy().setParams(wr.chosen);
        bt.strategy().train(MarketView(data, w.trainStart, w.trainEnd - 1));
        const BacktestResult test = bt.run(data, w.testStart, w.testEnd);

        // baseline on exactly the same bars
        Backtester base(StrategyFactory::create("BuyAndHold"), cost_, cfg_.initialEquity);
        const BacktestResult hold = base.run(data, w.testStart, w.testEnd);

        wr.used = true;
        wr.isSharpe = bestSharpe;
        wr.oosTrades = test.metrics.numTrades();
        rep.windows.push_back(wr);
        ++rep.usedWindows;
        rep.oosReturns.insert(rep.oosReturns.end(), test.returns.begin(), test.returns.end());
        rep.baselineReturns.insert(rep.baselineReturns.end(), hold.returns.begin(), hold.returns.end());
        oosTrades += test.metrics.numTrades();
        oosWins += countWins(test);
        exposureSum += test.metrics.exposure();
        turnoverSum += test.metrics.turnover();
        isSharpeSum += bestSharpe;
        rep.latestParams = wr.chosen;
        if (rep.periodStart == 0) rep.periodStart = data[0][w.testStart].openTime;
        rep.periodEnd = data[0][w.testEnd - 1].openTime;
    }

    const double ppy = PerformanceMetrics::periodsPerYear(data[0].interval());
    if (rep.usedWindows > 0) {
        const double nUsed = static_cast<double>(rep.usedWindows);
        rep.oos = PerformanceMetrics(PerformanceMetrics::equityFromReturns(rep.oosReturns), ppy);
        rep.oos.setTradeStats(oosTrades, oosTrades ? static_cast<double>(oosWins) / oosTrades : 0.0,
                              exposureSum / nUsed, turnoverSum / nUsed);
        rep.baseline = PerformanceMetrics(PerformanceMetrics::equityFromReturns(rep.baselineReturns), ppy);
        rep.isSharpe = isSharpeSum / nUsed;
        rep.overfitGap = rep.isSharpe - rep.oos.sharpe();
    }
    return rep;
}

}  // namespace quant
