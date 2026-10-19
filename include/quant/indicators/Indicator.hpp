#pragma once

#include <cmath>
#include <limits>

#include "quant/core/Candle.hpp"

namespace quant {

// Streaming indicator: feed one candle at a time, read value(). NaN until warmed up.
class Indicator {
public:
    virtual ~Indicator() = default;
    virtual double update(const Candle& c) = 0;
    virtual void reset() = 0;
    double value() const { return value_; }
    bool ready() const { return !std::isnan(value_); }

protected:
    double value_ = std::numeric_limits<double>::quiet_NaN();
};

// Indicators that only need one number per bar (the close). Strategies also use updatePrice()
// directly on derived series such as a spread.
class PriceIndicator : public Indicator {
public:
    virtual double updatePrice(double x) = 0;
    double update(const Candle& c) override { return updatePrice(c.close); }
};

}  // namespace quant
