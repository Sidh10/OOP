#include "quant/strategies/AdvancedStrategies.hpp"

#include <algorithm>
#include <cmath>

#include "quant/core/Errors.hpp"

namespace quant {

// ---------------- PairsTrading ----------------
PairsTradingStrategy::PairsTradingStrategy()
    : Strategy({{"window", 120}, {"entryZ", 2.0}, {"exitZ", 0.0}, {"stopZ", 4.0}, {"hedge", 0},
                {"kfDelta", 1e-5}, {"kfVe", 1e-3}}) {
    applyParams();
}

void PairsTradingStrategy::applyParams() {
    window_ = static_cast<size_t>(params_.get("window"));
    entryZ_ = params_.get("entryZ");
    exitZ_ = params_.get("exitZ");
    stopZ_ = params_.get("stopZ");
    kalman_ = params_.get("hedge") != 0.0;
    ols_.emplace(window_);
    spread_.emplace(window_);
    kf_ = KalmanFilter(params_.get("kfDelta"), params_.get("kfVe"));
}

void PairsTradingStrategy::reset() {
    ols_->reset();
    spread_->clear();
    kf_.reset();
    pos_ = 0;
    cooldown_ = 0;
    hedgeBeta_ = 1.0;
}

std::vector<TargetWeight> PairsTradingStrategy::onBar(const MarketView& view) {
    if (view.numSymbols() != 2) throw ConfigError("PairsTrading needs exactly two symbols");
    const double lnA = std::log(view.bar(0).close);
    const double lnB = std::log(view.bar(1).close);

    bool valid = false;
    double z = 0, beta = 0;
    if (!kalman_) {
        ols_->update(lnB, lnA);
        if (ols_->ready()) {
            const double s = lnA - (ols_->alpha() + ols_->beta() * lnB);
            spread_->push(s);
            if (spread_->full()) {
                const double sd = spread_->stddev();
                z = sd < 1e-12 ? 0.0 : (s - spread_->mean()) / sd;
                beta = ols_->beta();
                valid = true;
            }
        }
    } else {
        kf_.update(lnB, lnA);
        if (kf_.count() >= window_) {  // let the filter settle for `window` bars first
            // Standardise the innovation by its own rolling mean/std (scale free, like the OLS z).
            // e / sqrt(Q) alone has RMS ~0.1 on real hourly data because Q >= Ve, so it never
            // reaches entryZ.
            spread_->push(kf_.innovation());
            if (spread_->full()) {
                const double sd = spread_->stddev();
                z = sd < 1e-12 ? 0.0 : (kf_.innovation() - spread_->mean()) / sd;
                beta = kf_.beta();
                valid = true;
            }
        }
    }

    if (valid) {
        if (beta <= 0) {  // documented limitation: a negative hedge ratio is not traded
            pos_ = 0;
        } else if (cooldown_ > 0) {
            --cooldown_;
            pos_ = 0;
        } else if (pos_ != 0) {
            if (std::fabs(z) > stopZ_) {
                pos_ = 0;
                cooldown_ = window_ / 2;
            } else if ((pos_ == 1 && z >= -exitZ_) || (pos_ == -1 && z <= exitZ_)) {
                // Exit once z is back inside the exit band on the side it entered from. (With
                // exitZ = 0 the literal rule |z| < 0 could never fire, so z crossing zero exits.)
                pos_ = 0;
            }
        } else if (std::fabs(z) <= stopZ_) {  // do not open a trade that is already past the stop
            if (z < -entryZ_) {
                pos_ = 1;
                hedgeBeta_ = beta;
            } else if (z > entryZ_) {
                pos_ = -1;
                hedgeBeta_ = beta;
            }
        }
    }

    const double b = std::fabs(hedgeBeta_);
    const double p = static_cast<double>(pos_);
    return {{view.symbol(0), p / (1.0 + b)}, {view.symbol(1), -p * hedgeBeta_ / (1.0 + b)}};
}

// ---------------- MLFeatureBuilder ----------------
MLFeatureBuilder::MLFeatureBuilder() : closes_(21), rets_(20), rsi_(14), z20_(20), x_(kDim, 0.0) {}

void MLFeatureBuilder::reset() {
    closes_.clear();
    rets_.clear();
    rsi_.reset();
    z20_.reset();
}

bool MLFeatureBuilder::push(double close) {
    if (closes_.size() > 0) rets_.push(std::log(close / closes_.newest()));
    closes_.push(close);
    const double rsi = rsi_.updatePrice(close);
    const double z = z20_.updatePrice(close);
    if (!closes_.full() || !rets_.full() || std::isnan(rsi) || std::isnan(z)) return false;
    const size_t n = closes_.size();
    x_[0] = std::log(closes_.at(n - 1) / closes_.at(n - 2));  // 1-bar log return
    x_[1] = std::log(closes_.at(n - 1) / closes_.at(n - 6));  // 5-bar
    x_[2] = std::log(closes_.at(n - 1) / closes_.at(0));      // 20-bar
    x_[3] = (rsi - 50.0) / 50.0;
    x_[4] = z;
    x_[5] = rets_.stddev();
    return true;
}

// ---------------- MLSignal ----------------
MLSignalStrategy::MLSignalStrategy()
    : Strategy({{"lr", 0.05}, {"l2", 0.0}, {"threshold", 0.55}, {"online", 0}, {"longShort", 0}}),
      trained_(MLFeatureBuilder::kDim),
      model_(MLFeatureBuilder::kDim) {
    applyParams();
}

void MLSignalStrategy::applyParams() {
    lr_ = params_.get("lr");
    l2_ = params_.get("l2");
    threshold_ = params_.get("threshold");
    online_ = params_.get("online") != 0.0;
    longShort_ = params_.get("longShort") != 0.0;
    trained_ = LogisticRegression(MLFeatureBuilder::kDim, lr_, l2_, 50);
    model_ = trained_;
}

void MLSignalStrategy::train(const MarketView& view) {
    // Rebuild the (features, label) pairs from the visible window only. The label for bar t
    // (did the NEXT close go up?) is only usable once bar t+1 is inside the window.
    MLFeatureBuilder fb;
    std::vector<std::vector<double>> X;
    std::vector<double> y;
    std::vector<double> prevX;
    double prevClose = 0;
    bool hasPrev = false;
    for (size_t ago = view.count(); ago-- > 0;) {
        const double c = view.barAgo(0, ago).close;
        if (hasPrev) {
            X.push_back(prevX);
            y.push_back(c > prevClose ? 1.0 : 0.0);
        }
        hasPrev = fb.push(c);
        if (hasPrev) prevX = fb.features();
        prevClose = c;
    }
    trained_ = LogisticRegression(MLFeatureBuilder::kDim, lr_, l2_, 50);
    if (X.size() >= 50) trained_.fit(X, y);  // too little data: keep the neutral (all-zero) model
    model_ = trained_;
}

void MLSignalStrategy::reset() {
    model_ = trained_;  // drop anything learned online in a previous run
    features_.reset();
    hasPrev_ = false;
}

std::vector<TargetWeight> MLSignalStrategy::onBar(const MarketView& view) {
    const double close = view.bar(0).close;
    // y_{t-1} is known now that bar t has closed: update first, then predict.
    if (online_ && hasPrev_) model_.update(prevX_, close > prevClose_ ? 1.0 : 0.0);
    double w = 0.0;
    if (features_.push(close)) {
        const double p = model_.predict(features_.features());
        if (p > threshold_) w = 1.0;
        else if (longShort_ && p < 1.0 - threshold_) w = -1.0;
        prevX_ = features_.features();
        hasPrev_ = true;
    } else {
        hasPrev_ = false;
    }
    prevClose_ = close;
    return {{view.symbol(0), w}};
}

}  // namespace quant
