#pragma once

#include <optional>

#include "quant/core/RollingWindow.hpp"
#include "quant/indicators/Indicators.hpp"
#include "quant/models/Models.hpp"
#include "quant/strategies/Strategy.hpp"

namespace quant {

// Market-neutral spread trade on log prices: ln A = alpha + beta * ln B. The hedge ratio comes
// from a RollingOLS or a KalmanFilter (both owned by composition). Universe slot 0 is A, slot 1 is B.
class PairsTradingStrategy : public Strategy {
public:
    PairsTradingStrategy();
    std::string name() const override { return "PairsTrading"; }
    size_t warmupBars() const override { return 2 * window_; }
    std::vector<TargetWeight> onBar(const MarketView& view) override;
    void reset() override;
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<PairsTradingStrategy>(*this); }

private:
    void applyParams() override;
    size_t window_ = 120;
    double entryZ_ = 2.0, exitZ_ = 0.0, stopZ_ = 4.0;
    bool kalman_ = false;
    std::optional<RollingOLS> ols_;
    std::optional<RollingWindow<double>> spread_;
    KalmanFilter kf_;
    int pos_ = 0;            // +1 = long the spread (long A, short B), -1 = short the spread
    size_t cooldown_ = 0;    // bars to wait after a stop-out
    double hedgeBeta_ = 1.0; // frozen at entry so the position is not re-hedged (and re-charged) every bar
};

// Six features per bar, built incrementally from closes only (SPEC section 6).
class MLFeatureBuilder {
public:
    static constexpr size_t kDim = 6;
    MLFeatureBuilder();
    void reset();
    // Feed the next close; returns true once all six features are available.
    bool push(double close);
    const std::vector<double>& features() const { return x_; }

private:
    RollingWindow<double> closes_;  // last 21 closes
    RollingWindow<double> rets_;    // last 20 one-bar log returns
    RSI rsi_;
    ZScore z20_;
    std::vector<double> x_;
};

// Predicts whether the next bar closes up, using an online-capable logistic regression.
class MLSignalStrategy : public Strategy {
public:
    MLSignalStrategy();
    std::string name() const override { return "MLSignal"; }
    size_t warmupBars() const override { return 40; }
    void train(const MarketView& view) override;
    std::vector<TargetWeight> onBar(const MarketView& view) override;
    void reset() override;
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<MLSignalStrategy>(*this); }

private:
    void applyParams() override;
    double lr_ = 0.05, l2_ = 0.0, threshold_ = 0.55;
    bool online_ = false, longShort_ = false;
    LogisticRegression trained_;  // result of train(); reset() restores the live model from it
    LogisticRegression model_;
    MLFeatureBuilder features_;
    std::vector<double> prevX_;
    double prevClose_ = 0;
    bool hasPrev_ = false;
};

}  // namespace quant
