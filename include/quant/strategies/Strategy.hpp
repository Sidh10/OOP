#pragma once

#include <memory>
#include <string>
#include <vector>

#include "quant/strategies/MarketView.hpp"
#include "quant/strategies/ParamSet.hpp"

namespace quant {

// Desired portfolio weight for one symbol: +1 = fully long, -1 = fully short, 0 = flat.
struct TargetWeight {
    std::string symbol;
    double weight = 0.0;
};

// Lifecycle (see SPEC section 6): clone() per candidate, setParams(), train(view) once on the
// train window, then reset() + onBar(view) for each bar. onBar sees bars <= t and returns
// targets for the NEXT bar; the Backtester decides how and when they are executed.
class Strategy {
public:
    virtual ~Strategy() = default;

    virtual std::string name() const = 0;
    const ParamSet& params() const { return params_; }
    // Merges into the current params (defaults stay for keys not given) and resets state.
    void setParams(const ParamSet& p);

    // History the strategy needs before its first meaningful decision; the Backtester feeds
    // this many earlier bars (decisions ignored) so a test window can start with warm indicators.
    virtual size_t warmupBars() const { return 0; }
    virtual void train(const MarketView&) {}
    virtual std::vector<TargetWeight> onBar(const MarketView& view) = 0;
    // Clears streaming state but keeps parameters and anything learned in train().
    virtual void reset() = 0;
    virtual std::unique_ptr<Strategy> clone() const = 0;

protected:
    explicit Strategy(ParamSet defaults) : params_(std::move(defaults)) {}
    Strategy(const Strategy&) = default;
    Strategy& operator=(const Strategy&) = default;
    virtual void applyParams() = 0;  // rebuild indicators/models from params_

    ParamSet params_;
};

}  // namespace quant
