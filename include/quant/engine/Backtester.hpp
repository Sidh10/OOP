#pragma once

#include <memory>
#include <vector>

#include "quant/core/CandleSeries.hpp"
#include "quant/engine/BacktestResult.hpp"
#include "quant/engine/CostModel.hpp"
#include "quant/strategies/Strategy.hpp"

namespace quant {

// Event loop over aligned series. Timing rules (SPEC section 7), for each bar t:
//   1. execute pending targets (decided at the close of t-1) at the OPEN of t
//   2. mark the portfolio at the CLOSE of t
//   3. show bars <= t to the strategy and store its targets as pending for t+1
// The decision on the last bar is recorded but never executed.
class Backtester {
public:
    Backtester(std::unique_ptr<Strategy> strategy, std::shared_ptr<const CostModel> cost,
               double initialEquity = 10000.0);

    // The caller may configure or train the owned strategy before run().
    Strategy& strategy() { return *strategy_; }

    // Runs over absolute bars [from, to). The strategy first sees up to warmupBars() earlier bars
    // (decisions ignored) so indicators are warm; the portfolio itself starts flat at `from`.
    // May be called repeatedly (e.g. train window, then test window): state is reset every time.
    BacktestResult run(const std::vector<CandleSeries>& data, size_t from, size_t to);

private:
    std::unique_ptr<Strategy> strategy_;
    std::shared_ptr<const CostModel> cost_;
    double initialEquity_;
};

}  // namespace quant
