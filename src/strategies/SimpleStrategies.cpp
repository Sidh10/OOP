#include "quant/strategies/SimpleStrategies.hpp"

#include <algorithm>
#include <cmath>

#include "quant/core/Errors.hpp"

namespace quant {

// ---- BuyAndHold ----
std::vector<TargetWeight> BuyAndHoldStrategy::onBar(const MarketView& view) {
    // Equal weight over the universe (1 for a single asset, 50/50 for a pair) so the baseline
    // for a pair is a fair long-only holding of both legs.
    const double w = 1.0 / static_cast<double>(view.numSymbols());
    std::vector<TargetWeight> out;
    for (size_t k = 0; k < view.numSymbols(); ++k) out.push_back({view.symbol(k), w});
    return out;
}

// ---- MeanReversion ----
MeanReversionStrategy::MeanReversionStrategy()
    : Strategy({{"window", 50}, {"entryZ", 2.0}, {"exitZ", 0.0}, {"longShort", 0}}) {
    applyParams();
}

void MeanReversionStrategy::applyParams() {
    window_ = static_cast<size_t>(params_.get("window"));
    entryZ_ = params_.get("entryZ");
    exitZ_ = params_.get("exitZ");
    longShort_ = params_.get("longShort") != 0.0;
    z_.emplace(window_);
}

void MeanReversionStrategy::reset() {
    z_->reset();
    pos_ = 0;
}

std::vector<TargetWeight> MeanReversionStrategy::onBar(const MarketView& view) {
    const double z = z_->updatePrice(view.bar(0).close);
    if (!std::isnan(z)) {
        if (pos_ == 0) {
            if (z < -entryZ_) pos_ = 1;
            else if (longShort_ && z > entryZ_) pos_ = -1;
        } else if (pos_ == 1) {
            if (z > -exitZ_) pos_ = 0;
        } else if (z < exitZ_) {
            pos_ = 0;
        }
    }
    return {{view.symbol(0), static_cast<double>(pos_)}};
}

// ---- Momentum ----
MomentumStrategy::MomentumStrategy()
    : Strategy({{"fast", 10}, {"slow", 50}, {"longShort", 0}}) {
    applyParams();
}

void MomentumStrategy::applyParams() {
    fastN_ = static_cast<size_t>(params_.get("fast"));
    slowN_ = static_cast<size_t>(params_.get("slow"));
    longShort_ = params_.get("longShort") != 0.0;
    if (fastN_ >= slowN_) throw ConfigError("Momentum needs fast < slow");
    fast_.emplace(fastN_);
    slow_.emplace(slowN_);
}

void MomentumStrategy::reset() {
    fast_->reset();
    slow_->reset();
}

std::vector<TargetWeight> MomentumStrategy::onBar(const MarketView& view) {
    const double c = view.bar(0).close;
    const double f = fast_->updatePrice(c);
    const double s = slow_->updatePrice(c);
    double w = 0.0;
    if (!std::isnan(f) && !std::isnan(s)) w = f > s ? 1.0 : (longShort_ ? -1.0 : 0.0);
    return {{view.symbol(0), w}};
}

// ---- Breakout ----
BreakoutStrategy::BreakoutStrategy() : Strategy({{"N", 50}, {"M", 20}, {"longShort", 0}}) {
    applyParams();
}

void BreakoutStrategy::applyParams() {
    n_ = static_cast<size_t>(params_.get("N"));
    m_ = static_cast<size_t>(params_.get("M"));
    longShort_ = params_.get("longShort") != 0.0;
    if (m_ >= n_) throw ConfigError("Breakout needs M < N");
    highs_.emplace(n_);
    lows_.emplace(m_);
}

void BreakoutStrategy::reset() {
    highs_->clear();
    lows_->clear();
    pos_ = 0;
}

std::vector<TargetWeight> BreakoutStrategy::onBar(const MarketView& view) {
    const Candle& c = view.bar(0);
    if (highs_->full() && lows_->full()) {
        double hh = highs_->at(0), ll = lows_->at(0);
        for (size_t i = 1; i < highs_->size(); ++i) hh = std::max(hh, highs_->at(i));
        for (size_t i = 1; i < lows_->size(); ++i) ll = std::min(ll, lows_->at(i));
        if (c.close > hh) pos_ = 1;
        else if (c.close < ll) pos_ = longShort_ ? -1 : 0;
    }
    // Push after deciding: the channel must be built from the PREVIOUS bars only.
    highs_->push(c.high);
    lows_->push(c.low);
    return {{view.symbol(0), static_cast<double>(pos_)}};
}

}  // namespace quant
