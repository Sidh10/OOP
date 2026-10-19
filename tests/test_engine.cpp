#include <cmath>
#include <memory>

#include "minitest.hpp"
#include "quant/core/Errors.hpp"
#include "quant/engine/Backtester.hpp"
#include "quant/engine/CostModel.hpp"
#include "quant/engine/PerformanceMetrics.hpp"
#include "quant/engine/Portfolio.hpp"
#include "quant/strategies/SimpleStrategies.hpp"
#include "test_support.hpp"

using namespace quant;
using namespace testsupport;

namespace {

// SPEC section 12 test 7 data: open/high/low/close for 4 bars.
std::vector<CandleSeries> toyData() {
    CandleSeries s("TOY", "1h");
    const double o[] = {100, 100, 110, 121};
    const double h[] = {101, 111, 122, 122};
    const double l[] = {99, 99, 109, 100};
    const double c[] = {100, 110, 121, 110};
    for (int i = 0; i < 4; ++i) {
        Candle k;
        k.openTime = 1000 * H + i * H;
        k.closeTime = k.openTime + H - 1;
        k.open = o[i];
        k.high = h[i];
        k.low = l[i];
        k.close = c[i];
        k.volume = 1;
        s.push_back(k);
    }
    return {s};
}

std::shared_ptr<CostModel> costs(double feeBps, double slipBps) {
    return std::make_shared<BpsCostModel>(feeBps, slipBps);
}

}  // namespace

TEST(backtester_toy_dataset_zero_costs) {
    auto data = toyData();
    Backtester bt(std::make_unique<BuyAndHoldStrategy>(), costs(0, 0), 10000.0);
    BacktestResult r = bt.run(data, 0, 4);
    // Decision at close of bar 0 fills at bar 1 open = 100 -> 100 units.
    // Equity at closes: 10000 (flat), 100*110 = 11000, 100*121 = 12100, 100*110 = 11000.
    CHECK(r.equity.size() == 4);
    CHECK_NEAR(r.equity[0], 10000.0, 1e-9);
    CHECK_NEAR(r.equity[1], 11000.0, 1e-9);
    CHECK_NEAR(r.equity[2], 12100.0, 1e-9);
    CHECK_NEAR(r.equity[3], 11000.0, 1e-9);
    // Bar returns 0.1, 0.1, 11000/12100 - 1 = -0.0909090909...
    CHECK(r.returns.size() == 3);
    CHECK_NEAR(r.returns[0], 0.1, 1e-9);
    CHECK_NEAR(r.returns[1], 0.1, 1e-9);
    CHECK_NEAR(r.returns[2], -1.0 / 11.0, 1e-9);
    CHECK_NEAR(r.metrics.maxDrawdown(), 1100.0 / 12100.0, 1e-9);  // = 0.0909090909
    CHECK_NEAR(r.metrics.totalReturn(), 0.10, 1e-9);
    CHECK(r.fills.size() == 1);  // constant weight 1 is held, never re-balanced
    CHECK(r.fills[0].bar == 1);
    CHECK_NEAR(r.fills[0].fill.units, 100.0, 1e-9);
    CHECK(r.metrics.numTrades() == 1);  // one episode, still open at the end
    CHECK(!r.trades[0].closed);
}

TEST(backtester_cost_check_fee_10bps) {
    auto data = toyData();
    Backtester bt(std::make_unique<BuyAndHoldStrategy>(), costs(10, 0), 10000.0);
    BacktestResult r = bt.run(data, 0, 4);
    // Cash cannot go negative, so the buy is scaled: dV * (1 + 0.001) = 10000
    //   dV = 10000 / 1.001 = 9990.00999, units = dV / 100 = 99.9000999, fee = 9.99001, cash = 0.
    // Equity at close of bar 1 = 99.9000999 * 110 = 10989.010989.
    CHECK_NEAR(r.equity[1], 10989.010989, 1e-6);
    CHECK_NEAR(r.fills[0].fill.fee, 9.99000999, 1e-6);
    CHECK_NEAR(r.fills[0].fill.notional, 9990.00999, 1e-5);
    Portfolio p(10000.0, 1);
    p.rebalance({1.0}, {true}, {100.0}, BpsCostModel(10, 0));
    CHECK(p.cash() >= 0.0);
    CHECK_NEAR(p.cash(), 0.0, 1e-6);
}

TEST(backtester_slippage_makes_buys_dearer) {
    auto data = toyData();
    Backtester bt(std::make_unique<BuyAndHoldStrategy>(), costs(0, 20), 10000.0);  // 20 bps slippage
    BacktestResult r = bt.run(data, 0, 4);
    // Fill price 100 * 1.002 = 100.2; dV*(1.002) = 10000 -> dV = 9980.04, units = 99.8004.
    CHECK_NEAR(r.fills[0].fill.price, 100.2, 1e-9);
    CHECK_NEAR(r.fills[0].fill.units, 10000.0 / 100.2, 1e-9);
    CHECK(r.equity[1] < 11000.0);
}

TEST(execution_timing_fill_at_next_open) {
    // Signal flips from 0 to 1 at the close of bar 2 -> must fill at the OPEN of bar 3, not bar 2.
    std::vector<double> closes = {100, 101, 102, 103, 104, 105, 106};
    auto data = std::vector<CandleSeries>{seriesFromCloses("X", closes)};
    Backtester bt(std::make_unique<ScriptedStrategy>([](size_t t) { return t >= 2 ? 1.0 : 0.0; }),
                  costs(0, 0), 10000.0);
    BacktestResult r = bt.run(data, 0, closes.size());
    CHECK(r.fills.size() == 1);
    CHECK(r.fills[0].bar == 3);
    CHECK_NEAR(r.fills[0].fill.price, data[0][3].open, 1e-12);
    // Equity before the fill is flat, including at the close of bar 2 where the decision was made.
    CHECK_NEAR(r.equity[2], 10000.0, 1e-9);
    CHECK_NEAR(r.equity[3], 10000.0 / data[0][3].open * data[0][3].close, 1e-6);
}

TEST(last_bar_decision_is_never_executed) {
    std::vector<double> closes = {100, 101, 102, 103};
    auto data = std::vector<CandleSeries>{seriesFromCloses("X", closes)};
    Backtester bt(std::make_unique<ScriptedStrategy>([](size_t t) { return t >= 3 ? 1.0 : 0.0; }),
                  costs(0, 0), 10000.0);
    BacktestResult r = bt.run(data, 0, 4);
    CHECK(r.fills.empty());
    CHECK(r.metrics.numTrades() == 0);
}

TEST(episodes_and_win_rate) {
    // Two round trips: long for bars 1..2 (price rises), long for bars 5..6 (price falls).
    std::vector<double> closes = {100, 100, 110, 110, 110, 110, 100, 100, 100, 100};
    auto data = std::vector<CandleSeries>{seriesFromCloses("X", closes)};
    Backtester bt(std::make_unique<ScriptedStrategy>([](size_t t) { return (t == 0 || t == 1 || t == 4 || t == 5) ? 1.0 : 0.0; }),
                  costs(0, 0), 10000.0);
    BacktestResult r = bt.run(data, 0, closes.size());
    CHECK(r.trades.size() == 2);
    CHECK(r.trades[0].closed && r.trades[0].pnl > 0);
    CHECK(r.trades[1].closed && r.trades[1].pnl < 0);
    CHECK(r.metrics.numTrades() == 2);
    CHECK_NEAR(r.metrics.winRate(), 0.5, 1e-12);
}

TEST(portfolio_sells_first_and_never_overdraws) {
    Portfolio p(1000.0, 2);
    BpsCostModel c(10, 2);
    // buy A with everything
    p.rebalance({1.0, 0.0}, {true, true}, {10.0, 10.0}, c);
    CHECK(p.cash() >= 0 && p.units(0) > 0);
    // switch fully from A to B: proceeds of the sale fund the purchase, cash stays >= 0
    p.rebalance({0.0, 1.0}, {true, true}, {10.0, 10.0}, c);
    CHECK(p.cash() >= 0);
    CHECK(p.units(0) == 0.0);
    CHECK(p.units(1) > 0);
    // equity only shrinks by costs (two round-trip sides of 12 bps on ~1000)
    CHECK(p.equity({10.0, 10.0}) < 1000.0 && p.equity({10.0, 10.0}) > 995.0);
}

TEST(portfolio_gross_exposure_capped_at_one) {
    Portfolio p(1000.0, 2);
    BpsCostModel c(0, 0);
    p.rebalance({1.0, 1.0}, {true, true}, {10.0, 10.0}, c);  // asks for 200% gross
    CHECK_NEAR(p.grossValue({10.0, 10.0}), 1000.0, 1e-9);
    CHECK_NEAR(p.units(0), 50.0, 1e-9);
}

TEST(portfolio_short_sale_raises_cash_and_marks_to_market) {
    Portfolio p(1000.0, 1);
    BpsCostModel c(0, 0);
    p.rebalance({-1.0}, {true}, {10.0}, c);
    CHECK_NEAR(p.units(0), -100.0, 1e-9);
    CHECK_NEAR(p.cash(), 2000.0, 1e-9);
    CHECK_NEAR(p.equity({9.0}), 2000.0 - 900.0, 1e-9);  // price fell 10% -> +100 profit
    p.rebalance({0.0}, {true}, {9.0}, c);               // cover
    CHECK(p.isFlat());
    CHECK_NEAR(p.cash(), 1100.0, 1e-9);
}

TEST(metrics_hand_computed) {
    // equity 100, 110, 99, 108.9 -> returns +0.1, -0.1, +0.1 (periodsPerYear = 16 so sqrt = 4)
    //   mean = 0.033333, sample std = 0.115470, downside dev = sqrt(0.01/3) = 0.057735
    //   Sharpe = 0.033333/0.115470*4 = 1.154701, Sortino = 0.033333/0.057735*4 = 2.309401
    //   maxDD = (110-99)/110 = 0.1, totalReturn = 0.089, CAGR = 1.089^(16/4) - 1 = 0.406409
    PerformanceMetrics m({100, 110, 99, 108.9}, 16.0);
    CHECK_NEAR(m.sharpe(), 1.1547005383792515, 1e-9);
    CHECK_NEAR(m.sortino(), 2.309401076758503, 1e-9);
    CHECK_NEAR(m.maxDrawdown(), 0.1, 1e-12);
    CHECK_NEAR(m.totalReturn(), 0.089, 1e-12);
    CHECK_NEAR(m.cagr(), 0.40640861824099983, 1e-9);
    CHECK_NEAR(PerformanceMetrics::periodsPerYear("1h"), 8760.0, 1e-9);
    // flat equity -> zero std -> Sharpe 0, not NaN
    PerformanceMetrics flat({100, 100, 100}, 16.0);
    CHECK_NEAR(flat.sharpe(), 0.0, 0);
    auto eq = PerformanceMetrics::equityFromReturns({0.1, -0.1});
    CHECK_NEAR(eq[2], 0.99, 1e-12);
}

TEST(unaligned_series_rejected) {
    auto a = seriesFromCloses("A", {1, 2, 3, 4});
    auto b = seriesFromCloses("B", {1, 2, 3, 4}, 2000 * H);  // different timestamps
    Backtester bt(std::make_unique<BuyAndHoldStrategy>(), costs(0, 0));
    CHECK_THROWS(bt.run({a, b}, 0, 4), DataError);
}
