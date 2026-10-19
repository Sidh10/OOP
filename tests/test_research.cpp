#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <set>
#include <sstream>

#include "minitest.hpp"
#include "quant/core/Errors.hpp"
#include "quant/engine/CostModel.hpp"
#include "quant/research/ReportWriter.hpp"
#include "quant/research/StrategyRecommender.hpp"
#include "quant/research/WalkForwardOptimizer.hpp"
#include "test_support.hpp"

using namespace quant;
using namespace testsupport;

namespace {
std::shared_ptr<CostModel> defaultCosts() {
    return std::make_shared<BpsCostModel>(defaults::feeBps, defaults::slippageBps);
}

// lnA = 0.5 + 1.2 lnB + OU noise: cointegrated by construction (seeded).
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

TEST(walk_forward_window_boundaries) {
    WalkForwardConfig cfg;
    cfg.trainBars = 2000;
    cfg.testBars = 500;
    cfg.stepBars = 500;
    auto w = WalkForwardOptimizer::makeWindows(5000, cfg);
    // s = 0, 500, 1000, ... while s + 2500 <= 5000  ->  s = 0, 500, 1000, 1500, 2000, 2500
    CHECK(w.size() == 6);
    CHECK(w[0].trainStart == 0 && w[0].trainEnd == 2000 && w[0].testStart == 2000 && w[0].testEnd == 2500);
    CHECK(w[5].trainStart == 2500 && w[5].testEnd == 5000);
    for (size_t i = 0; i < w.size(); ++i) {
        CHECK(w[i].testStart == w[i].trainEnd);  // test starts right after its own train window
        CHECK(w[i].testEnd - w[i].testStart == 500 && w[i].trainEnd - w[i].trainStart == 2000);
        if (i > 0) {
            CHECK(w[i].testStart >= w[i - 1].testEnd);  // test windows never overlap
            CHECK(w[i].testStart == w[i - 1].testEnd);  // and with step == test they tile exactly
        }
    }
    // no train bar of any window lies inside THAT window's test range, and test bars of one window
    // never appear in its own train range
    for (const auto& x : w) CHECK(x.trainEnd <= x.testStart);
    CHECK(WalkForwardOptimizer::makeWindows(2499, cfg).empty());
    CHECK(WalkForwardOptimizer::makeWindows(2500, cfg).size() == 1);
}

TEST(walk_forward_runs_and_is_consistent) {
    auto closes = randomWalk(2400, 11, 100.0, 0.01, 0.0003);
    std::vector<CandleSeries> data{seriesFromCloses("X", closes)};
    WalkForwardConfig cfg;
    cfg.trainBars = 800;
    cfg.testBars = 300;
    cfg.stepBars = 300;
    WalkForwardOptimizer wf(cfg, defaultCosts());
    WalkForwardReport rep = wf.run("Momentum", data);
    CHECK(rep.windows.size() == WalkForwardOptimizer::makeWindows(2400, cfg).size());
    CHECK(rep.universe == "X");
    for (const auto& w : rep.windows) {
        if (!w.used) continue;
        CHECK(w.window.testStart == w.window.trainEnd);
        CHECK(!w.chosen.values().empty());
    }
    // stitched series: (testBars - 1) returns per used window, baseline on the same bars
    CHECK(rep.oosReturns.size() == rep.usedWindows * (cfg.testBars - 1));
    CHECK(rep.baselineReturns.size() == rep.oosReturns.size());
    if (rep.usedWindows > 0) CHECK(rep.periodEnd > rep.periodStart);
    CHECK_THROWS(wf.run("BuyAndHold", data), ConfigError);  // the baseline has no grid
}

TEST(walk_forward_skips_windows_without_enough_trades) {
    // A flat price never triggers a single trade: every window must be skipped.
    std::vector<CandleSeries> data{seriesFromCloses("X", std::vector<double>(2000, 100.0))};
    WalkForwardConfig cfg;
    cfg.trainBars = 600;
    cfg.testBars = 200;
    cfg.stepBars = 200;
    WalkForwardOptimizer wf(cfg, defaultCosts());
    auto rep = wf.run("Breakout", data);
    CHECK(rep.usedWindows == 0);
    CHECK(rep.oosReturns.empty());
    auto rec = StrategyRecommender::toRecommendation(rep, 20);
    CHECK(rec.verdict == Verdict::Insufficient);
}

TEST(verdict_rules) {
    // INSUFFICIENT below 20 trades regardless of Sharpe
    CHECK(StrategyRecommender::decide(19, 3.0, 0.0, 0.0) == Verdict::Insufficient);
    // RECOMMENDED needs: sharpe > baseline + 0.2, sharpe > 0.5, gap <= 1.0
    CHECK(StrategyRecommender::decide(30, 1.0, 0.5, 0.5) == Verdict::Recommended);
    CHECK(StrategyRecommender::decide(30, 1.0, 0.9, 0.5) == Verdict::Weak);         // does not beat baseline by 0.2
    CHECK(StrategyRecommender::decide(30, 0.4, -1.0, 0.0) == Verdict::Weak);        // below the 0.5 floor
    CHECK(StrategyRecommender::decide(30, 1.0, 0.0, 1.5) == Verdict::Weak);         // overfit gap too large
    CHECK(StrategyRecommender::decide(30, -0.1, -2.0, 0.0) == Verdict::Reject);
    CHECK_NEAR(StrategyRecommender::score(1.0, 0.2, 0.4), 1.0 - 0.2 - 0.2, 1e-12);
    CHECK_NEAR(StrategyRecommender::score(1.0, 0.2, -0.4), 0.8, 1e-12);  // negative gap is not rewarded
}

TEST(pairs_make_money_out_of_sample_on_cointegrated_synthetic_pair) {
    auto data = cointegratedPair(4000, 42);
    WalkForwardConfig cfg;
    cfg.trainBars = 1000;
    cfg.testBars = 250;
    cfg.stepBars = 250;
    WalkForwardOptimizer wf(cfg, defaultCosts());
    WalkForwardReport rep = wf.run("PairsTrading", data);
    std::cerr << "  [pairs synthetic] windows used " << rep.usedWindows << ", OOS return "
              << rep.oos.totalReturn() * 100 << "%, Sharpe " << rep.oos.sharpe() << ", trades "
              << rep.oos.numTrades() << "\n";
    CHECK(rep.usedWindows >= 5);
    CHECK(rep.oos.numTrades() > 20);
    CHECK(rep.oos.totalReturn() > 0.0);  // after 10 bps fee + 2 bps slippage per side, on both legs
}

TEST(honesty_random_walk_is_never_recommended) {
    // Pure random walks (drift 0), seed 42: nothing real to find, so nothing may be RECOMMENDED.
    std::vector<CandleSeries> series;
    series.push_back(seriesFromCloses("AAAUSDT", randomWalk(7000, 42, 100.0, 0.01)));
    series.push_back(seriesFromCloses("BBBUSDT", randomWalk(7000, 43, 100.0, 0.01)));
    StrategyRecommender rec(WalkForwardConfig{}, defaultCosts());
    RecommendationSet set = rec.rank(series, {{"AAAUSDT", "BBBUSDT"}});
    CHECK(!set.rows.empty());
    for (const auto& r : set.rows)
        std::cerr << "  [honesty] " << r.universe << " " << r.strategy << " " << toString(r.verdict)
                  << " oosSharpe=" << r.oosSharpe << " base=" << r.baselineSharpe << " trades=" << r.oosTrades
                  << "\n";
    CHECK(!set.anyRecommended());
}

TEST(report_csv_columns_and_round_trip) {
    RecommendationSet set;
    Recommendation a;
    a.rank = 1;
    a.universe = "ETHUSDT/BTCUSDT";
    a.strategy = "PairsTrading";
    a.verdict = Verdict::Weak;
    a.score = 0.123456;
    a.oosSharpe = 0.7;
    a.oosTrades = 42;
    a.paramsLatest = "entryZ=2;window=120";
    a.windows = 7;
    a.periodStart = "2025-01-01 00:00Z";
    a.periodEnd = "2026-01-01 00:00Z";
    Recommendation b;
    b.universe = "BTCUSDT";
    b.strategy = "Momentum";
    b.verdict = Verdict::Insufficient;  // unranked, no params, no period
    set.rows = {a, b};
    const std::string path = "logs/_test_report.csv";
    ReportWriter::writeCsv(path, set.rows);

    std::ifstream in(path);
    std::string header;
    std::getline(in, header);
    CHECK(header ==
          "rank,universe,strategy,verdict,score,oos_sharpe,oos_sortino,oos_cagr,oos_maxdd,oos_trades,"
          "oos_winrate,is_sharpe,overfit_gap,baseline_sharpe,baseline_cagr,params_latest,n_windows,"
          "period_start,period_end");
    auto back = ReportWriter::readCsv(path);
    CHECK(back.size() == 2);
    CHECK(back[0].rank == 1 && back[0].universe == "ETHUSDT/BTCUSDT" && back[0].verdict == Verdict::Weak);
    CHECK_NEAR(back[0].score, 0.123456, 1e-9);
    CHECK(back[0].paramsLatest == "entryZ=2;window=120" && back[0].windows == 7);
    CHECK(back[0].periodStart == "2025-01-01 00:00Z");
    CHECK(back[1].rank == 0 && back[1].verdict == Verdict::Insufficient && back[1].paramsLatest.empty());
    std::remove(path.c_str());
    CHECK_THROWS(ReportWriter::readCsv("logs/_no_such_report.csv"), DataError);
}

TEST(report_text_is_honest_when_nothing_is_recommended) {
    RecommendationSet set;
    Recommendation r;
    r.rank = 1;
    r.universe = "BTCUSDT";
    r.strategy = "Momentum";
    r.verdict = Verdict::Reject;
    set.rows = {r};
    std::ostringstream os;
    ReportWriter::printReport(os, set);
    const std::string t = os.str();
    CHECK(t.find("no strategy beat the buy-and-hold baseline out of sample on this data") != std::string::npos);
    CHECK(t.find("Research output, not financial advice. Past performance does not predict future results.") !=
          std::string::npos);
    CHECK(t.find("NO borrow or funding cost") != std::string::npos);
    CHECK(t.find("multiple testing") != std::string::npos);
}
