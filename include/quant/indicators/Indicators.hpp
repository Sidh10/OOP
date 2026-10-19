#pragma once

#include "quant/core/RollingWindow.hpp"
#include "quant/indicators/Indicator.hpp"

namespace quant {

class SMA : public PriceIndicator {
public:
    explicit SMA(size_t n);
    double updatePrice(double x) override;
    void reset() override;

private:
    RollingWindow<double> win_;
};

// alpha = 2/(n+1), seeded with the SMA of the first n values.
class EMA : public PriceIndicator {
public:
    explicit EMA(size_t n);
    double updatePrice(double x) override;
    void reset() override;

private:
    size_t n_;
    double alpha_;
    size_t count_ = 0;
    double seedSum_ = 0;
};

// Wilder's RSI, 0..100. Needs n price changes (n+1 prices) before it is ready.
class RSI : public PriceIndicator {
public:
    explicit RSI(size_t n);
    double updatePrice(double x) override;
    void reset() override;

private:
    size_t n_;
    size_t changes_ = 0;
    bool hasPrev_ = false;
    double prev_ = 0, avgGain_ = 0, avgLoss_ = 0;
};

// (x - mean_n) / sample std_n; 0 when the window is flat.
class ZScore : public PriceIndicator {
public:
    explicit ZScore(size_t n);
    double updatePrice(double x) override;
    void reset() override;

private:
    RollingWindow<double> win_;
};

// Wilder smoothing of the true range. The first bar has no previous close, so its TR is high-low.
class ATR : public Indicator {
public:
    explicit ATR(size_t n);
    double update(const Candle& c) override;
    void reset() override;

private:
    size_t n_;
    size_t count_ = 0;
    bool hasPrev_ = false;
    double prevClose_ = 0, atr_ = 0;
};

}  // namespace quant
