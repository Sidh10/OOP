#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

#include "minitest.hpp"
#include "quant/core/Errors.hpp"
#include "quant/indicators/Indicators.hpp"

using namespace quant;

namespace {
const double NaN = std::numeric_limits<double>::quiet_NaN();

std::vector<Candle> randomBars(int n, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> step(0.0, 1.0);
    std::uniform_real_distribution<double> wick(0.0, 1.5);
    std::vector<Candle> out;
    double px = 100;
    for (int i = 0; i < n; ++i) {
        Candle c;
        c.open = px;
        c.close = px + step(rng);
        c.high = std::max(c.open, c.close) + wick(rng);
        c.low = std::min(c.open, c.close) - wick(rng);
        px = c.close;
        out.push_back(c);
    }
    return out;
}

// Independent reference versions, written from arrays rather than streaming state.
double naiveMean(const std::vector<double>& v, size_t end, size_t n) {
    double s = 0;
    for (size_t i = end - n; i < end; ++i) s += v[i];
    return s / static_cast<double>(n);
}
double naiveStd(const std::vector<double>& v, size_t end, size_t n) {
    double m = naiveMean(v, end, n), s = 0;
    for (size_t i = end - n; i < end; ++i) s += (v[i] - m) * (v[i] - m);
    return std::sqrt(s / static_cast<double>(n - 1));
}
bool same(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
    return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(b));
}
}  // namespace

TEST(sma_hand_check) {
    SMA s(3);
    std::vector<double> out;
    for (double x : {1.0, 2.0, 3.0, 4.0, 5.0}) out.push_back(s.updatePrice(x));
    CHECK(std::isnan(out[0]) && std::isnan(out[1]));
    CHECK_NEAR(out[2], 2, 1e-12);
    CHECK_NEAR(out[3], 3, 1e-12);
    CHECK_NEAR(out[4], 4, 1e-12);
    CHECK(s.ready());
    s.reset();
    CHECK(!s.ready());
}

TEST(indicators_match_naive_reference) {
    auto bars = randomBars(400, 7);
    std::vector<double> closes;
    for (const auto& b : bars) closes.push_back(b.close);
    const size_t n = 14;

    SMA sma(n);
    EMA ema(n);
    RSI rsi(n);
    ZScore z(n);
    ATR atr(n);

    // naive EMA / RSI / ATR state, rebuilt from arrays
    const double alpha = 2.0 / (n + 1.0);
    double emaRef = NaN;
    double avgG = 0, avgL = 0, atrRef = NaN;
    std::vector<double> tr;
    for (size_t i = 0; i < bars.size(); ++i) {
        double vS = sma.update(bars[i]);
        double vE = ema.update(bars[i]);
        double vR = rsi.update(bars[i]);
        double vZ = z.update(bars[i]);
        double vA = atr.update(bars[i]);

        double refS = i + 1 >= n ? naiveMean(closes, i + 1, n) : NaN;
        CHECK(same(vS, refS));

        if (i + 1 == n) emaRef = naiveMean(closes, n, n);
        else if (i + 1 > n) emaRef = alpha * closes[i] + (1 - alpha) * emaRef;
        CHECK(same(vE, i + 1 >= n ? emaRef : NaN));

        double refR = NaN;
        if (i >= 1) {
            double ch = closes[i] - closes[i - 1];
            double g = std::max(ch, 0.0), l = std::max(-ch, 0.0);
            if (i < n) {
                avgG += g;
                avgL += l;
            } else if (i == n) {
                avgG = (avgG + g) / n;  // first n changes are i=1..n
                avgL = (avgL + l) / n;
                refR = avgL == 0 ? 100.0 : 100.0 - 100.0 / (1.0 + avgG / avgL);
            } else {
                avgG = (avgG * (n - 1) + g) / n;
                avgL = (avgL * (n - 1) + l) / n;
                refR = avgL == 0 ? 100.0 : 100.0 - 100.0 / (1.0 + avgG / avgL);
            }
        }
        CHECK(same(vR, refR));

        double refZ = NaN;
        if (i + 1 >= n) {
            double sd = naiveStd(closes, i + 1, n);
            refZ = sd < 1e-12 ? 0.0 : (closes[i] - naiveMean(closes, i + 1, n)) / sd;
        }
        CHECK(same(vZ, refZ));

        double t = bars[i].high - bars[i].low;
        if (i > 0)
            t = std::max({t, std::fabs(bars[i].high - closes[i - 1]), std::fabs(bars[i].low - closes[i - 1])});
        tr.push_back(t);
        if (i + 1 == n) atrRef = naiveMean(tr, n, n);
        else if (i + 1 > n) atrRef = (atrRef * (n - 1) + t) / n;
        CHECK(same(vA, i + 1 >= n ? atrRef : NaN));
    }
    CHECK(rsi.value() >= 0 && rsi.value() <= 100);
}

TEST(rsi_and_atr_hand_checks) {
    // RSI(2) on 1,2,1,2: changes +1,-1 -> avgGain=avgLoss=0.5 -> 50; then +1 -> gain 0.75, loss 0.25 -> 75
    RSI r(2);
    r.updatePrice(1);
    r.updatePrice(2);
    CHECK(!r.ready());
    CHECK_NEAR(r.updatePrice(1), 50.0, 1e-12);
    CHECK_NEAR(r.updatePrice(2), 75.0, 1e-12);
    // Monotone rise: no losses -> 100
    RSI up(3);
    double v = NaN;
    for (double x : {1.0, 2.0, 3.0, 4.0, 5.0}) v = up.updatePrice(x);
    CHECK_NEAR(v, 100.0, 1e-12);

    // ATR(2): TR1=2, TR2=2 -> 2; next TR=4 -> (2*1+4)/2 = 3
    ATR a(2);
    Candle c1, c2, c3;
    c1.high = 10; c1.low = 8; c1.close = 9;
    c2.high = 11; c2.low = 9; c2.close = 10;
    c3.high = 14; c3.low = 10; c3.close = 13;
    a.update(c1);
    CHECK_NEAR(a.update(c2), 2.0, 1e-12);
    CHECK_NEAR(a.update(c3), 3.0, 1e-12);
}

TEST(zscore_flat_window_is_zero_and_ema_seed) {
    ZScore z(5);
    for (int i = 0; i < 5; ++i) z.updatePrice(10.0);
    CHECK_NEAR(z.value(), 0.0, 0);
    // EMA(3): seed = mean(1,2,3) = 2; then alpha=0.5: 0.5*4 + 0.5*2 = 3
    EMA e(3);
    e.updatePrice(1);
    e.updatePrice(2);
    CHECK(!e.ready());
    CHECK_NEAR(e.updatePrice(3), 2.0, 1e-12);
    CHECK_NEAR(e.updatePrice(4), 3.0, 1e-12);
    CHECK_THROWS(EMA(0), ConfigError);
}

TEST(indicators_polymorphic_through_base_pointer) {
    std::vector<std::unique_ptr<Indicator>> v;
    v.push_back(std::make_unique<SMA>(3));
    v.push_back(std::make_unique<ATR>(3));
    Candle c;
    c.open = c.high = c.low = c.close = 5;
    for (int i = 0; i < 3; ++i)
        for (auto& ind : v) ind->update(c);
    CHECK(v[0]->ready() && v[1]->ready());
    CHECK_NEAR(v[0]->value(), 5, 1e-12);
    CHECK_NEAR(v[1]->value(), 0, 1e-12);
}
