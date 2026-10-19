#include <cmath>
#include <random>

#include "minitest.hpp"
#include "quant/core/Errors.hpp"
#include "quant/models/Models.hpp"
#include "quant/strategies/AdvancedStrategies.hpp"
#include "test_support.hpp"

using namespace quant;
using namespace testsupport;

TEST(rolling_ols_recovers_alpha_beta) {
    std::mt19937 rng(42);
    std::normal_distribution<double> xs(10.0, 1.0), noise(0.0, 0.01);
    RollingOLS ols(200);
    for (int i = 0; i < 500; ++i) {
        double x = xs(rng);
        ols.update(x, 2.0 + 1.5 * x + noise(rng));
    }
    CHECK(ols.ready());
    CHECK_NEAR(ols.beta(), 1.5, 0.01);
    CHECK_NEAR(ols.alpha(), 2.0, 0.1);
    CHECK_NEAR(ols.residualStd(), 0.01, 0.003);
    // the window forgets: after a regime change the estimate follows the new slope
    for (int i = 0; i < 400; ++i) {
        double x = xs(rng);
        ols.update(x, -1.0 + 0.5 * x + noise(rng));
    }
    CHECK_NEAR(ols.beta(), 0.5, 0.01);
    CHECK_NEAR(ols.alpha(), -1.0, 0.1);
    CHECK_NEAR(ols.predict({10.0}), -1.0 + 5.0, 0.05);
    CHECK_THROWS(RollingOLS(2), ConfigError);
}

TEST(rolling_ols_keeps_estimate_when_regressor_is_constant) {
    RollingOLS ols(10);
    for (int i = 0; i < 10; ++i) ols.update(1.0 * i, 3.0 + 2.0 * i);
    CHECK_NEAR(ols.beta(), 2.0, 1e-9);
    for (int i = 0; i < 9; ++i) ols.update(5.0, 100.0);  // window still holds one varied point
    const double b = ols.beta();
    ols.update(5.0, 100.0);  // now every x in the window is 5: variance 0
    ols.update(5.0, 100.0);
    CHECK_NEAR(ols.beta(), b, 0);  // previous estimate kept, no division by ~0
    CHECK(std::isfinite(ols.beta()) && std::isfinite(ols.alpha()));
}

TEST(kalman_beta_converges_on_synthetic_pair) {
    std::mt19937 rng(42);
    std::normal_distribution<double> step(0.0, 0.01), obs(0.0, 0.01);
    KalmanFilter kf(1e-5, 1e-3);
    double lnB = 4.0;
    for (int i = 0; i < 3000; ++i) {
        lnB += step(rng);
        kf.update(lnB, 0.3 + 1.2 * lnB + obs(rng));
    }
    CHECK_NEAR(kf.beta(), 1.2, 0.1);
    CHECK(kf.innovationStd() > 0);
    CHECK(std::isfinite(kf.zscore()));
    kf.reset();
    CHECK(kf.count() == 0);
}

TEST(kalman_tracks_a_slow_beta_drift) {
    std::mt19937 rng(7);
    std::normal_distribution<double> step(0.0, 0.02), obs(0.0, 0.002);
    KalmanFilter kf(1e-4, 1e-4);
    double lnB = 3.0;
    for (int i = 0; i < 4000; ++i) {
        lnB += step(rng);
        double beta = 1.0 + 0.5 * i / 4000.0;  // drifts from 1.0 to 1.5
        kf.update(lnB, beta * lnB + obs(rng));
    }
    CHECK_NEAR(kf.beta(), 1.5, 0.15);
}

TEST(logistic_regression_separates_linearly_separable_set) {
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<std::vector<double>> X;
    std::vector<double> y;
    for (int i = 0; i < 400; ++i) {
        double label = i % 2;
        double shift = label ? 3.0 : -3.0;  // classes 6 sigma apart along feature 0
        X.push_back({shift + nd(rng), nd(rng) * 5.0 + 100.0});
        y.push_back(label);
    }
    LogisticRegression lr(2, 0.5, 0.0, 300);
    lr.fit(X, y);
    int correct = 0;
    for (size_t i = 0; i < X.size(); ++i)
        if ((lr.predict(X[i]) > 0.5) == (y[i] > 0.5)) ++correct;
    CHECK(static_cast<double>(correct) / X.size() > 0.95);
    // fresh held-out points
    int ok2 = 0;
    for (int i = 0; i < 200; ++i) {
        double label = i % 2;
        std::vector<double> x{(label ? 3.0 : -3.0) + nd(rng), nd(rng) * 5.0 + 100.0};
        if ((lr.predict(x) > 0.5) == (label > 0.5)) ++ok2;
    }
    CHECK(ok2 / 200.0 > 0.95);
}

TEST(logistic_regression_online_update_moves_probability) {
    LogisticRegression lr(1, 0.1, 0.0, 10);
    std::vector<std::vector<double>> X{{-1}, {1}, {-1}, {1}};
    std::vector<double> y{0, 1, 0, 1};
    lr.fit(X, y);
    double before = lr.predict({1.0});
    for (int i = 0; i < 50; ++i) lr.update({1.0}, 0.0);  // contradicting evidence
    CHECK(lr.predict({1.0}) < before);
    CHECK_THROWS(lr.fit({{1.0}}, {1.0, 0.0}), DataError);
}

TEST(models_are_usable_through_the_base_class) {
    std::vector<std::unique_ptr<Model>> models;
    models.push_back(std::make_unique<RollingOLS>(50));
    models.push_back(std::make_unique<KalmanFilter>());
    std::mt19937 rng(1);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<std::vector<double>> X;
    std::vector<double> y;
    for (int i = 0; i < 300; ++i) {
        double x = nd(rng) + 5;
        X.push_back({x});
        y.push_back(1.0 + 2.0 * x + 0.01 * nd(rng));
    }
    for (auto& m : models) {
        m->fit(X, y);  // default fit() replays the pairs through update()
        CHECK_NEAR(m->predict({6.0}), 13.0, 0.3);
    }
}

TEST(dickey_fuller_separates_stationary_from_random_walk) {
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<double> ar, rw;
    double a = 0, r = 0;
    for (int i = 0; i < 1000; ++i) {
        a = 0.5 * a + nd(rng);
        r += nd(rng);
        ar.push_back(a);
        rw.push_back(r);
    }
    CHECK(dickeyFullerStat(ar) < -3.34);  // clearly mean-reverting
    CHECK(dickeyFullerStat(rw) > -3.34);  // fails to reject a unit root (seeded, deterministic)
    CHECK_THROWS(dickeyFullerStat({1, 2, 3}), DataError);
}

TEST(ml_features_ready_after_21_closes_and_hand_check) {
    MLFeatureBuilder fb;
    bool ready = false;
    for (int i = 0; i < 20; ++i) ready = fb.push(100.0 * std::pow(1.01, i));
    CHECK(!ready);
    ready = fb.push(100.0 * std::pow(1.01, 20));  // 21st close
    CHECK(ready);
    const auto& x = fb.features();
    CHECK_NEAR(x[0], std::log(1.01), 1e-12);          // 1-bar log return
    CHECK_NEAR(x[1], 5 * std::log(1.01), 1e-12);      // 5-bar
    CHECK_NEAR(x[2], 20 * std::log(1.01), 1e-12);     // 20-bar
    CHECK_NEAR(x[3], 1.0, 1e-12);                     // RSI = 100 on a pure uptrend -> (100-50)/50
    CHECK_NEAR(x[5], 0.0, 1e-9);                      // constant returns -> zero std
}

namespace {
// lnA = 0.5 + 1.2 lnB + OU noise: cointegrated by construction.
std::vector<CandleSeries> cointegratedPair(size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<double> a, b;
    double lnB = std::log(100.0), u = 0.0;
    for (size_t i = 0; i < n; ++i) {
        lnB += 0.01 * nd(rng);
        u = 0.9 * u + 0.012 * nd(rng);
        b.push_back(std::exp(lnB));
        a.push_back(std::exp(0.5 + 1.2 * lnB + u));
    }
    return {seriesFromCloses("AAAUSDT", a), seriesFromCloses("BBBUSDT", b)};
}
}  // namespace

TEST(pairs_strategy_trades_both_legs_with_opposite_signs) {
    auto data = cointegratedPair(3000, 42);
    for (double hedge : {0.0, 1.0}) {
        PairsTradingStrategy s;
        s.setParams({{"window", 60}, {"entryZ", 2.0}, {"exitZ", 0.0}, {"stopZ", 4.0}, {"hedge", hedge}});
        size_t entries = 0;
        int prev = 0;
        for (size_t t = 0; t < data[0].size(); ++t) {
            auto w = s.onBar(MarketView(data, 0, t));
            CHECK(w.size() == 2);
            CHECK(std::fabs(w[0].weight) + std::fabs(w[1].weight) <= 1.0 + 1e-12);  // gross exposure <= 1
            CHECK(w[0].weight * w[1].weight <= 0.0);                                // hedge leg is opposite
            int cur = w[0].weight > 0 ? 1 : (w[0].weight < 0 ? -1 : 0);
            if (prev == 0 && cur != 0) ++entries;
            prev = cur;
        }
        CHECK(entries >= 5);  // a mean-reverting spread must generate trades
    }
    PairsTradingStrategy one;
    std::vector<CandleSeries> single{data[0]};
    CHECK_THROWS(one.onBar(MarketView(single, 0, 0)), ConfigError);
}
