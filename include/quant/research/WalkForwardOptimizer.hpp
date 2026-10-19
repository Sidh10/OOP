#pragma once

#include <memory>
#include <string>
#include <vector>

#include "quant/Defaults.hpp"
#include "quant/core/CandleSeries.hpp"
#include "quant/engine/CostModel.hpp"
#include "quant/engine/PerformanceMetrics.hpp"
#include "quant/strategies/ParamSet.hpp"

namespace quant {

struct WalkForwardConfig {
    size_t trainBars = defaults::wfTrainBars;
    size_t testBars = defaults::wfTestBars;
    size_t stepBars = defaults::wfStepBars;
    int minTradesTrain = defaults::minTradesTrain;
    double initialEquity = defaults::initialEquity;
};

// Train [trainStart, trainEnd), test [testStart, testEnd) with testStart == trainEnd.
struct WfWindow {
    size_t trainStart = 0, trainEnd = 0, testStart = 0, testEnd = 0;
};

struct WfWindowResult {
    WfWindow window;
    bool used = false;      // false: no parameter set reached minTradesTrain, window skipped
    ParamSet chosen;
    double isSharpe = 0;    // in-sample Sharpe of the chosen parameters
    int oosTrades = 0;
};

struct WalkForwardReport {
    std::string strategy;
    std::string universe;                 // "BTCUSDT" or "ETHUSDT/BTCUSDT"
    std::vector<WfWindowResult> windows;
    size_t usedWindows = 0;
    std::vector<double> oosReturns;       // stitched out-of-sample bar returns
    std::vector<double> baselineReturns;  // buy-and-hold over exactly the same windows
    PerformanceMetrics oos;
    PerformanceMetrics baseline;
    double isSharpe = 0;                  // mean in-sample Sharpe of the selected parameters
    double overfitGap = 0;                // isSharpe - oos Sharpe
    ParamSet latestParams;                // selection of the last used window
    int64_t periodStart = 0, periodEnd = 0;  // first / last out-of-sample bar (ms), 0 if none
};

// Rolling walk-forward (SPEC section 8): choose parameters on a train window by grid search,
// score them only on the following unseen test window, stitch all test windows together.
class WalkForwardOptimizer {
public:
    WalkForwardOptimizer(WalkForwardConfig cfg, std::shared_ptr<const CostModel> cost);

    static std::vector<WfWindow> makeWindows(size_t numBars, const WalkForwardConfig& cfg);

    // `data` are aligned series: one for single-asset strategies, two (A, B) for pairs.
    WalkForwardReport run(const std::string& strategyName, const std::vector<CandleSeries>& data) const;

private:
    WalkForwardConfig cfg_;
    std::shared_ptr<const CostModel> cost_;
};

}  // namespace quant
