// Helpers shared by tests: synthetic series builders and a scripted test strategy.
#pragma once

#include <cmath>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "quant/core/CandleSeries.hpp"
#include "quant/strategies/Strategy.hpp"

namespace testsupport {

constexpr int64_t H = 3600000;

// Builds bars from closes: open = previous close, small wicks. All values positive.
inline quant::CandleSeries seriesFromCloses(const std::string& symbol, const std::vector<double>& closes,
                                            int64_t t0 = 1000 * H) {
    quant::CandleSeries s(symbol, "1h");
    double prev = closes.empty() ? 1.0 : closes[0];
    for (size_t i = 0; i < closes.size(); ++i) {
        quant::Candle c;
        c.openTime = t0 + static_cast<int64_t>(i) * H;
        c.closeTime = c.openTime + H - 1;
        c.open = prev;
        c.close = closes[i];
        c.high = std::max(c.open, c.close) * 1.002;
        c.low = std::min(c.open, c.close) * 0.998;
        c.volume = 10;
        prev = closes[i];
        s.push_back(c);
    }
    return s;
}

inline std::vector<double> randomWalk(size_t n, unsigned seed, double start = 100.0, double vol = 0.01,
                                      double drift = 0.0) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(drift, vol);
    std::vector<double> out;
    double px = start;
    for (size_t i = 0; i < n; ++i) {
        px *= std::exp(nd(rng));
        out.push_back(px);
    }
    return out;
}

// Test double: returns whatever the supplied function says for the current absolute bar index.
class ScriptedStrategy : public quant::Strategy {
public:
    using Script = std::function<double(size_t barIndex)>;
    explicit ScriptedStrategy(Script s) : quant::Strategy({}), script_(std::move(s)) {}
    std::string name() const override { return "Scripted"; }
    std::vector<quant::TargetWeight> onBar(const quant::MarketView& v) override {
        return {{v.symbol(0), script_(v.index())}};
    }
    void reset() override {}
    std::unique_ptr<quant::Strategy> clone() const override { return std::make_unique<ScriptedStrategy>(*this); }

private:
    void applyParams() override {}
    Script script_;
};

// Synthetic Binance-style kline page: n rows from open time t0, one hour apart, in the 12-field layout.
inline std::string klinesJson(int64_t t0, int n, double price = 100.0) {
    std::ostringstream os;
    os << "[";
    for (int i = 0; i < n; ++i) {
        int64_t t = t0 + i * H;
        if (i) os << ",";
        os << "[" << t << ",\"" << price << "\",\"" << price * 1.01 << "\",\"" << price * 0.99 << "\",\""
           << price * 1.005 << "\",\"1.5\"," << (t + H - 1) << ",\"0\",1,\"0\",\"0\",\"0\"]";
    }
    os << "]";
    return os.str();
}

}  // namespace testsupport
