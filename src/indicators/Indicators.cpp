#include "quant/indicators/Indicators.hpp"

#include <algorithm>

#include "quant/core/Errors.hpp"

namespace quant {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
void needPositive(size_t n, const char* name) {
    if (n == 0) throw ConfigError(std::string(name) + " period must be > 0");
}
}  // namespace

// ---- SMA ----
SMA::SMA(size_t n) : win_(n) {}
double SMA::updatePrice(double x) {
    win_.push(x);
    value_ = win_.full() ? win_.mean() : kNaN;
    return value_;
}
void SMA::reset() {
    win_.clear();
    value_ = kNaN;
}

// ---- EMA ----
EMA::EMA(size_t n) : n_(n), alpha_(n ? 2.0 / (static_cast<double>(n) + 1.0) : 0.0) {
    needPositive(n, "EMA");
}
double EMA::updatePrice(double x) {
    if (count_ < n_) {
        seedSum_ += x;
        if (++count_ == n_) value_ = seedSum_ / static_cast<double>(n_);
    } else {
        value_ = alpha_ * x + (1.0 - alpha_) * value_;
    }
    return value_;
}
void EMA::reset() {
    count_ = 0;
    seedSum_ = 0;
    value_ = kNaN;
}

// ---- RSI ----
RSI::RSI(size_t n) : n_(n) { needPositive(n, "RSI"); }
double RSI::updatePrice(double x) {
    if (!hasPrev_) {
        hasPrev_ = true;
        prev_ = x;
        return value_;
    }
    const double change = x - prev_;
    prev_ = x;
    const double gain = std::max(change, 0.0);
    const double loss = std::max(-change, 0.0);
    const double dn = static_cast<double>(n_);
    if (changes_ < n_) {
        avgGain_ += gain;
        avgLoss_ += loss;
        if (++changes_ == n_) {
            avgGain_ /= dn;
            avgLoss_ /= dn;
        } else {
            return value_;
        }
    } else {
        avgGain_ = (avgGain_ * (dn - 1.0) + gain) / dn;
        avgLoss_ = (avgLoss_ * (dn - 1.0) + loss) / dn;
    }
    if (avgLoss_ == 0.0)
        value_ = avgGain_ == 0.0 ? 50.0 : 100.0;
    else
        value_ = 100.0 - 100.0 / (1.0 + avgGain_ / avgLoss_);
    return value_;
}
void RSI::reset() {
    changes_ = 0;
    hasPrev_ = false;
    prev_ = avgGain_ = avgLoss_ = 0;
    value_ = kNaN;
}

// ---- ZScore ----
ZScore::ZScore(size_t n) : win_(n) {}
double ZScore::updatePrice(double x) {
    win_.push(x);
    if (!win_.full()) return value_;
    const double sd = win_.stddev();
    value_ = sd < 1e-12 ? 0.0 : (x - win_.mean()) / sd;
    return value_;
}
void ZScore::reset() {
    win_.clear();
    value_ = kNaN;
}

// ---- ATR ----
ATR::ATR(size_t n) : n_(n) { needPositive(n, "ATR"); }
double ATR::update(const Candle& c) {
    double tr = c.high - c.low;
    if (hasPrev_) tr = std::max({tr, std::fabs(c.high - prevClose_), std::fabs(c.low - prevClose_)});
    hasPrev_ = true;
    prevClose_ = c.close;
    const double dn = static_cast<double>(n_);
    if (count_ < n_) {
        atr_ += tr;
        if (++count_ == n_) {
            atr_ /= dn;
            value_ = atr_;
        }
    } else {
        atr_ = (atr_ * (dn - 1.0) + tr) / dn;
        value_ = atr_;
    }
    return value_;
}
void ATR::reset() {
    count_ = 0;
    hasPrev_ = false;
    prevClose_ = atr_ = 0;
    value_ = kNaN;
}

}  // namespace quant
