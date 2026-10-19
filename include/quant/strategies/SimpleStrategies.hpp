#pragma once

#include <optional>

#include "quant/core/RollingWindow.hpp"
#include "quant/indicators/Indicators.hpp"
#include "quant/strategies/Strategy.hpp"

namespace quant {

// Baseline: weight 1 from the first bar. Never a candidate for recommendation.
class BuyAndHoldStrategy : public Strategy {
public:
    BuyAndHoldStrategy() : Strategy({}) {}
    std::string name() const override { return "BuyAndHold"; }
    std::vector<TargetWeight> onBar(const MarketView& view) override;
    void reset() override {}
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<BuyAndHoldStrategy>(*this); }

private:
    void applyParams() override {}
};

// Buy after an unusually low z-score, sell when it has recovered. The strategy owns its
// ZScore indicator (composition) instead of inheriting from it.
class MeanReversionStrategy : public Strategy {
public:
    MeanReversionStrategy();
    std::string name() const override { return "MeanReversion"; }
    size_t warmupBars() const override { return window_; }
    std::vector<TargetWeight> onBar(const MarketView& view) override;
    void reset() override;
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<MeanReversionStrategy>(*this); }

private:
    void applyParams() override;
    std::optional<ZScore> z_;
    size_t window_ = 50;
    double entryZ_ = 2.0, exitZ_ = 0.0;
    bool longShort_ = false;
    int pos_ = 0;
};

// Long while the fast SMA is above the slow SMA.
class MomentumStrategy : public Strategy {
public:
    MomentumStrategy();
    std::string name() const override { return "Momentum"; }
    size_t warmupBars() const override { return slowN_; }
    std::vector<TargetWeight> onBar(const MarketView& view) override;
    void reset() override;
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<MomentumStrategy>(*this); }

private:
    void applyParams() override;
    std::optional<SMA> fast_, slow_;
    size_t fastN_ = 10, slowN_ = 50;
    bool longShort_ = false;
};

// Donchian-style breakout. Enter long above the highest high of the previous N bars (the current
// bar is excluded), exit below the lowest low of the previous M bars. With longShort=1 the exit
// signal flips to short instead (stop-and-reverse).
class BreakoutStrategy : public Strategy {
public:
    BreakoutStrategy();
    std::string name() const override { return "Breakout"; }
    size_t warmupBars() const override { return n_; }
    std::vector<TargetWeight> onBar(const MarketView& view) override;
    void reset() override;
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<BreakoutStrategy>(*this); }

private:
    void applyParams() override;
    std::optional<RollingWindow<double>> highs_, lows_;
    size_t n_ = 50, m_ = 20;
    bool longShort_ = false;
    int pos_ = 0;
};

}  // namespace quant
