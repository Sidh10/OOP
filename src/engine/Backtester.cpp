#include "quant/engine/Backtester.hpp"

#include <algorithm>
#include <cmath>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"

namespace quant {

Backtester::Backtester(std::unique_ptr<Strategy> strategy, std::shared_ptr<const CostModel> cost,
                       double initialEquity)
    : strategy_(std::move(strategy)), cost_(std::move(cost)), initialEquity_(initialEquity) {
    if (!strategy_) throw ConfigError("Backtester needs a strategy");
    if (!cost_) throw ConfigError("Backtester needs a cost model");
    if (initialEquity_ <= 0) throw ConfigError("initial equity must be > 0");
}

BacktestResult Backtester::run(const std::vector<CandleSeries>& data, size_t from, size_t to) {
    if (data.empty()) throw DataError("Backtester::run: no data");
    const size_t n = data.size();
    if (from >= to || to > data[0].size()) throw DataError("Backtester::run: bad bar range");
    const size_t warm = std::min(from, strategy_->warmupBars());
    const size_t begin = from - warm;
    for (size_t k = 1; k < n; ++k) {
        if (data[k].size() != data[0].size()) throw DataError("series are not aligned (different lengths)");
        for (size_t t = begin; t < to; ++t)
            if (data[k][t].openTime != data[0][t].openTime)
                throw DataError("series are not aligned at bar " + std::to_string(t) + "; call alignSeries first");
    }

    strategy_->reset();
    // Warm-up: bars before `from` only feed the strategy's state. Their decisions are discarded,
    // and nothing is carried into the portfolio, so every run starts flat.
    for (size_t t = begin; t < from; ++t) strategy_->onBar(MarketView(data, begin, t));

    Portfolio pf(initialEquity_, n);
    BacktestResult res;
    res.strategyName = strategy_->name();
    res.params = strategy_->params().toString();
    res.firstBar = from;

    std::vector<double> open(n), close(n);
    std::vector<double> pending(n, 0.0), lastTarget(n, 0.0);
    std::vector<bool> trade(n, false);
    bool havePending = false;

    bool inEpisode = false;
    double cashAtEntry = 0;
    size_t entryBar = 0;
    double exposureSum = 0, turnoverSum = 0, equitySum = 0;

    for (size_t t = from; t < to; ++t) {
        for (size_t k = 0; k < n; ++k) {
            open[k] = data[k][t].open;
            close[k] = data[k][t].close;
        }

        // 1) execute what was decided at the close of t-1, at the open of t
        if (havePending) {
            bool any = false;
            for (size_t k = 0; k < n; ++k) {
                // Only re-trade a slot when its target changed; constant weights are held, not
                // re-balanced every bar (avoids paying fees for price drift).
                trade[k] = std::fabs(pending[k] - lastTarget[k]) > 1e-12;
                any = any || trade[k];
            }
            if (any) {
                const bool wasFlat = pf.isFlat();
                const double cashBefore = pf.cash();
                std::vector<Fill> fills = pf.rebalance(pending, trade, open, *cost_);
                for (const Fill& f : fills) {
                    res.fills.push_back({t, f});
                    turnoverSum += f.notional;
                }
                for (size_t k = 0; k < n; ++k)
                    if (trade[k]) lastTarget[k] = pending[k];
                if (wasFlat && !pf.isFlat()) {
                    inEpisode = true;
                    cashAtEntry = cashBefore;  // flat, so equity == cash before the entry costs
                    entryBar = t;
                } else if (!wasFlat && pf.isFlat() && inEpisode) {
                    res.trades.push_back({entryBar, t, pf.cash() - cashAtEntry, true});
                    inEpisode = false;
                }
            }
        }

        // 2) mark at the close
        const double eq = pf.equity(close);
        res.equity.push_back(eq);
        equitySum += eq;
        exposureSum += eq > 0 ? pf.grossValue(close) / eq : 0.0;

        // 3) decide with bars <= t; the result is pending for t+1
        std::vector<TargetWeight> tw = strategy_->onBar(MarketView(data, begin, t));
        std::fill(pending.begin(), pending.end(), 0.0);  // symbols not mentioned mean "flat"
        for (const TargetWeight& w : tw) {
            bool found = false;
            for (size_t k = 0; k < n && !found; ++k)
                if (data[k].symbol() == w.symbol) {
                    pending[k] = w.weight;
                    found = true;
                }
            if (!found) throw DataError("strategy returned weight for unknown symbol " + w.symbol);
        }
        havePending = true;
        res.targets.push_back(pending);
    }

    if (inEpisode)  // still exposed at the end: count it, marked at the final close
        res.trades.push_back({entryBar, to - 1, res.equity.back() - cashAtEntry, false});

    res.returns = PerformanceMetrics::returnsOf(res.equity);
    const double bars = static_cast<double>(res.equity.size());
    res.metrics = PerformanceMetrics(res.equity, PerformanceMetrics::periodsPerYear(data[0].interval()));
    int wins = 0;
    for (const auto& tr : res.trades)
        if (tr.pnl > 0) ++wins;
    const int nt = static_cast<int>(res.trades.size());
    const double meanEq = equitySum / bars;
    res.metrics.setTradeStats(nt, nt ? static_cast<double>(wins) / nt : 0.0, exposureSum / bars,
                              meanEq > 0 ? turnoverSum / meanEq : 0.0);
    return res;
}

}  // namespace quant
