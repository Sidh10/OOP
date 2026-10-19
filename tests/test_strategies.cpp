#include <cmath>
#include <memory>
#include <random>
#include <set>

#include "minitest.hpp"
#include "quant/core/Errors.hpp"
#include "quant/engine/Backtester.hpp"
#include "quant/research/ParameterGrid.hpp"
#include "quant/strategies/SimpleStrategies.hpp"
#include "quant/strategies/StrategyFactory.hpp"
#include "test_support.hpp"

using namespace quant;
using namespace testsupport;

namespace {

// Streams a strategy over all bars and returns the decision made at each bar's close.
std::vector<std::vector<double>> weightsOver(Strategy& s, const std::vector<CandleSeries>& data,
                                             size_t trainEnd) {
    if (trainEnd > 0) s.train(MarketView(data, 0, trainEnd));
    s.reset();
    std::vector<std::vector<double>> out;
    for (size_t t = 0; t < data[0].size(); ++t) {
        std::vector<double> row(data.size(), 0.0);
        MarketView v(data, 0, t);
        for (const auto& w : s.onBar(v)) row[v.slotOf(w.symbol)] = w.weight;
        out.push_back(row);
    }
    return out;
}

}  // namespace

TEST(param_set_and_grid) {
    ParamSet p{{"a", 1.5}, {"b", 2}};
    CHECK(p.toString() == "a=1.5;b=2");
    ParamSet q = ParamSet::fromString("a=1.5;b=2");
    CHECK(p == q);
    CHECK_THROWS(p.get("zzz"), ConfigError);
    CHECK_NEAR(p.get("zzz", 7.0), 7.0, 0);
    CHECK_THROWS(ParamSet::fromString("novalue"), ParseError);

    ParameterGrid g;
    g.axis("x", {1, 2, 3}).axis("y", {10, 20});
    CHECK(g.combinations().size() == 6);
    g.require([](const ParamSet& s) { return s.get("x") < 3; });
    CHECK(g.combinations().size() == 4);
    CHECK(ParameterGrid().combinations().empty());
}

TEST(factory_creates_every_registered_strategy) {
    auto names = StrategyFactory::registered();
    CHECK(names.size() >= 4);
    std::set<std::string> seen;
    for (const auto& n : names) {
        auto s = StrategyFactory::create(n);
        CHECK(s != nullptr);
        CHECK(s->name() == n);
        seen.insert(n);
        for (const auto& p : StrategyFactory::grid(n).combinations()) {
            auto c = s->clone();
            c->setParams(p);  // every grid point must be a valid configuration
            CHECK(c->name() == n);
        }
    }
    CHECK(seen.count("BuyAndHold") == 1);
    CHECK(StrategyFactory::candidates().size() + 1 == names.size());
    CHECK_THROWS(StrategyFactory::create("Nope"), ConfigError);
    // grid sizes from SPEC section 6
    CHECK(StrategyFactory::grid("MeanReversion").combinations().size() == 18);
    CHECK(StrategyFactory::grid("Breakout").combinations().size() == 6);
    CHECK(StrategyFactory::grid("Momentum").combinations().size() == 14);  // 4 + 4 + 4 + 2 pairs with fast < slow
}

TEST(no_lookahead_for_every_registered_strategy) {
    // Same prices up to bar k, garbage afterwards: decisions for bars <= k must be identical.
    const size_t N = 700, k = 450, trainEnd = 300;
    auto a = randomWalk(N, 42, 100.0, 0.01);
    auto b = randomWalk(N, 43, 50.0, 0.01);
    auto a2 = a, b2 = b;
    std::mt19937 rng(99);
    std::uniform_real_distribution<double> junk(1.0, 1000.0);  // wildly different future
    for (size_t i = k + 1; i < N; ++i) {
        a2[i] = junk(rng);
        b2[i] = junk(rng);
    }
    std::vector<CandleSeries> S{seriesFromCloses("AAAUSDT", a), seriesFromCloses("BBBUSDT", b)};
    std::vector<CandleSeries> G{seriesFromCloses("AAAUSDT", a2), seriesFromCloses("BBBUSDT", b2)};

    for (const auto& name : StrategyFactory::registered()) {
        auto combos = StrategyFactory::grid(name).combinations();
        std::vector<ParamSet> tests{ParamSet{}};
        if (!combos.empty()) {
            tests.push_back(combos.front());
            tests.push_back(combos[combos.size() / 2]);
            tests.push_back(combos.back());
        }
        for (const auto& p : tests) {
            auto s1 = StrategyFactory::create(name);
            auto s2 = StrategyFactory::create(name);
            s1->setParams(p);
            s2->setParams(p);
            auto w1 = weightsOver(*s1, S, trainEnd);
            auto w2 = weightsOver(*s2, G, trainEnd);
            bool same = true;
            for (size_t t = 0; t <= k; ++t)
                for (size_t j = 0; j < w1[t].size(); ++j)
                    if (w1[t][j] != w2[t][j]) same = false;
            if (!same) std::cerr << "  leakage suspected in " << name << " [" << p.toString() << "]\n";
            CHECK(same);
        }
    }
}

TEST(mean_reversion_buys_a_dip_and_exits) {
    std::vector<double> c(60, 100.0);
    for (size_t i = 0; i < c.size(); ++i) c[i] = 100.0 + (i % 2 ? 0.5 : -0.5);  // gentle noise
    c[40] = 90.0;  // sharp drop -> very negative z-score
    c[41] = 100.0;
    c[42] = 101.0;
    auto data = std::vector<CandleSeries>{seriesFromCloses("X", c)};
    MeanReversionStrategy s;
    s.setParams({{"window", 20}, {"entryZ", 2.0}, {"exitZ", 0.0}});
    auto w = weightsOver(s, data, 0);
    CHECK(w[39][0] == 0.0);
    CHECK(w[40][0] == 1.0);  // z << -2 -> long
    CHECK(w[42][0] == 0.0);  // back above the mean -> flat again
}

TEST(momentum_follows_the_trend) {
    std::vector<double> up, down;
    for (int i = 0; i < 80; ++i) up.push_back(100 + i);
    for (int i = 0; i < 80; ++i) down.push_back(200 - i);
    MomentumStrategy s;
    s.setParams({{"fast", 5}, {"slow", 20}});
    auto wu = weightsOver(s, {seriesFromCloses("X", up)}, 0);
    auto wd = weightsOver(s, {seriesFromCloses("X", down)}, 0);
    CHECK(wu[10][0] == 0.0);  // slow SMA not ready yet
    CHECK(wu.back()[0] == 1.0);
    CHECK(wd.back()[0] == 0.0);
    s.setParams({{"longShort", 1}});
    auto wd2 = weightsOver(s, {seriesFromCloses("X", down)}, 0);
    CHECK(wd2.back()[0] == -1.0);
    CHECK_THROWS(s.setParams({{"fast", 30}, {"slow", 20}}), ConfigError);
}

TEST(breakout_excludes_current_bar_and_exits_on_low) {
    // 30 flat bars, then a jump (breakout), later a collapse (exit)
    std::vector<double> c(30, 100.0);
    c.push_back(120.0);                       // bar 30: close above the previous 20 highs
    for (int i = 0; i < 10; ++i) c.push_back(120.0);
    c.push_back(80.0);                        // bar 41: below the previous 10 lows
    c.push_back(80.0);
    auto data = std::vector<CandleSeries>{seriesFromCloses("X", c)};
    BreakoutStrategy s;
    s.setParams({{"N", 20}, {"M", 10}});
    auto w = weightsOver(s, data, 0);
    CHECK(w[29][0] == 0.0);
    CHECK(w[30][0] == 1.0);   // the channel never includes the breakout bar itself
    CHECK(w[40][0] == 1.0);
    CHECK(w[41][0] == 0.0);   // exit
    CHECK_THROWS(s.setParams({{"N", 10}, {"M", 20}}), ConfigError);
}

TEST(strategies_run_through_backtester_polymorphically) {
    auto closes = randomWalk(1500, 5, 100.0, 0.01, 0.0002);
    std::vector<CandleSeries> data{seriesFromCloses("X", closes)};
    auto cost = std::make_shared<BpsCostModel>(10, 2);
    for (const auto& name : StrategyFactory::candidates()) {
        if (StrategyFactory::isPairStrategy(name)) continue;
        Backtester bt(StrategyFactory::create(name), cost, 10000.0);
        BacktestResult r = bt.run(data, 300, 1400);
        CHECK(r.equity.size() == 1100);
        CHECK(r.metrics.maxDrawdown() >= 0 && r.metrics.maxDrawdown() <= 1.0);
        for (double e : r.equity) CHECK(e > 0 && std::isfinite(e));
    }
}

namespace {
// Deliberately cheats: reads a close 10 bars ahead through a raw pointer, bypassing MarketView.
class CheatingStrategy : public Strategy {
public:
    explicit CheatingStrategy(const std::vector<CandleSeries>* d) : Strategy({}), data_(d) {}
    std::string name() const override { return "Cheat"; }
    std::vector<TargetWeight> onBar(const MarketView& v) override {
        size_t t = v.index();
        double w = (t + 10 < (*data_)[0].size() && (*data_)[0][t + 10].close > (*data_)[0][t].close) ? 1.0 : 0.0;
        return {{v.symbol(0), w}};
    }
    void reset() override {}
    std::unique_ptr<Strategy> clone() const override { return std::make_unique<CheatingStrategy>(*this); }

private:
    void applyParams() override {}
    const std::vector<CandleSeries>* data_;
};
}  // namespace

TEST(leakage_check_has_teeth_it_catches_a_cheating_strategy) {
    const size_t N = 300, k = 150;
    auto a = randomWalk(N, 1);
    auto a2 = a;
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> junk(1.0, 1000.0);
    for (size_t i = k + 1; i < N; ++i) a2[i] = junk(rng);
    std::vector<CandleSeries> S{seriesFromCloses("X", a)}, G{seriesFromCloses("X", a2)};
    CheatingStrategy c1(&S), c2(&G);
    auto w1 = weightsOver(c1, S, 0);
    auto w2 = weightsOver(c2, G, 0);
    bool same = true;
    for (size_t t = 0; t <= k; ++t)
        if (w1[t][0] != w2[t][0]) same = false;
    CHECK(!same);  // the identical-weights test would fail for this strategy, as it should
}

TEST(market_view_exposes_only_visible_bars) {
    std::vector<CandleSeries> data{seriesFromCloses("X", {1, 2, 3, 4, 5, 6, 7, 8})};
    MarketView v(data, 2, 5);  // visible: bars 2..5
    CHECK(v.count() == 4 && v.index() == 5);
    CHECK_NEAR(v.bar(0).close, 6, 0);
    CHECK_NEAR(v.barAgo(0, 3).close, 3, 0);                 // oldest visible bar
    CHECK_THROWS(v.barAgo(0, 4), DataError);                // bar 1 is hidden (before `begin`)
    CandleSeries h = v.history("X", 100);                   // asks for more than exists: clamped
    CHECK(h.size() == 4);
    CHECK_NEAR(h.back().close, 6, 0);                       // bar 6 and later never appear
    CHECK(v.history(0, 2).size() == 2);
    CHECK_THROWS(v.slotOf("NOPE"), DataError);
    CHECK_THROWS(MarketView(data, 6, 5), DataError);        // begin > index
    CHECK_THROWS(MarketView(data, 0, 8), DataError);        // index beyond the data
}
