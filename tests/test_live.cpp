#include <cstdio>
#include <fstream>
#include <sstream>

#include "minitest.hpp"
#include "quant/Config.hpp"
#include "quant/core/Errors.hpp"
#include "quant/core/Logger.hpp"
#include "quant/live/LiveRunner.hpp"
#include "test_support.hpp"

using namespace quant;
using namespace testsupport;

namespace {

// A "world" of pre-generated candles of which only the first `visible` are closed. advance()
// makes the next candle close, like time passing on the exchange.
class FakeDataSource : public IDataSource {
public:
    explicit FakeDataSource(std::vector<CandleSeries> world, size_t visible)
        : world_(std::move(world)), visible_(visible) {}
    void advance(size_t n = 1) { visible_ += n; }
    int failuresLeft = 0;  // next calls throw NetworkError while > 0
    int latestCalls = 0;

    CandleSeries fetchHistory(const std::string& symbol, const std::string& interval, int64_t fromMs,
                              int64_t toMs) override {
        const CandleSeries& s = find(symbol);
        CandleSeries out(symbol, interval);
        for (size_t i = 0; i < visible_ && i < s.size(); ++i)
            if (s[i].openTime >= fromMs && s[i].openTime <= toMs) out.push_back(s[i]);
        return out;
    }
    Candle fetchLatest(const std::string& symbol, const std::string&) override {
        ++latestCalls;
        if (failuresLeft > 0) {
            --failuresLeft;
            throw NetworkError("simulated outage");
        }
        return find(symbol)[visible_ - 1];
    }
    int64_t nowMs() const { return world_[0][visible_ - 1].closeTime + 1; }

private:
    const CandleSeries& find(const std::string& sym) const {
        for (const auto& s : world_)
            if (s.symbol() == sym) return s;
        throw DataError("fake: unknown symbol " + sym);
    }
    std::vector<CandleSeries> world_;
    size_t visible_;
};

struct Collector : ISignalListener {
    std::vector<SignalEvent> events;
    void onSignal(const SignalEvent& e) override { events.push_back(e); }
};

Recommendation choice(const std::string& universe, const std::string& strategy, const std::string& params) {
    Recommendation r;
    r.rank = 1;
    r.universe = universe;
    r.strategy = strategy;
    r.paramsLatest = params;
    r.verdict = Verdict::Recommended;
    return r;
}

// up for 400 bars, then down: a fast/slow SMA momentum strategy goes long, then flat.
std::vector<double> upThenDown() {
    std::vector<double> c;
    for (int i = 0; i < 400; ++i) c.push_back(100 + 0.5 * i);
    for (int i = 0; i < 400; ++i) c.push_back(300 - 0.5 * i);
    return c;
}

std::shared_ptr<FakeDataSource> momentumWorld(size_t visible) {
    return std::make_shared<FakeDataSource>(std::vector<CandleSeries>{seriesFromCloses("AAAUSDT", upThenDown())},
                                            visible);
}

}  // namespace

TEST(config_defaults_and_overrides) {
    Config d;
    CHECK(d.symbols.size() == 4 && d.symbols[0] == "BTCUSDT");
    CHECK(d.pairs.size() == 3 && d.pairs[0].first == "ETHUSDT" && d.pairs[0].second == "BTCUSDT");
    CHECK(d.interval == "1h" && d.historyDays == 730);
    CHECK_NEAR(d.feeBps, 10, 0);
    CHECK_NEAR(d.slippageBps, 2, 0);
    CHECK(d.wfTrain == 2000 && d.wfTest == 500 && d.wfStep == 500);
    CHECK(d.pollSeconds == 30);

    Config c = Config::fromText(
        "# my settings\n"
        "symbols = BTCUSDT, ETHUSDT   # two only\n"
        "pairs = ETHUSDT/BTCUSDT\n"
        "fee_bps=7.5\n"
        "history_days = 100\n"
        "wf_train=1000\n"
        "\n"
        "poll_seconds = 5\n");
    CHECK(c.symbols.size() == 2 && c.symbols[1] == "ETHUSDT");
    CHECK(c.pairs.size() == 1);
    CHECK_NEAR(c.feeBps, 7.5, 0);
    CHECK(c.historyDays == 100 && c.wfTrain == 1000 && c.pollSeconds == 5);
    CHECK(c.wfTest == 500);  // untouched keys keep their defaults
    CHECK(c.walkForward().trainBars == 1000);
    CHECK(Config::load("logs/_does_not_exist_config.txt").symbols.size() == 4);  // optional file
}

TEST(config_bad_lines_throw_config_error_with_line_number) {
    CHECK_THROWS(Config::fromText("symbols=BTCUSDT\nthis is not valid\n"), ConfigError);
    CHECK_THROWS(Config::fromText("fee_bps=abc\n"), ConfigError);
    CHECK_THROWS(Config::fromText("colour=blue\n"), ConfigError);
    CHECK_THROWS(Config::fromText("pairs=ETHUSDT\n"), ConfigError);
    CHECK_THROWS(Config::fromText("history_days=-5\n"), ConfigError);
    CHECK_THROWS(Config::fromText("symbols=\n"), ConfigError);
    try {
        Config::fromText("a=1\n", "myfile.txt");
        CHECK(false);
    } catch (const ConfigError& e) {
        std::string m = e.what();
        CHECK(m.find("myfile.txt line 1") != std::string::npos);
    }
}

TEST(describe_targets_and_console_line) {
    CHECK(describeTargets({{"X", 0.0}}) == "FLAT");
    CHECK(describeTargets({{"X", 1.0}}) == "LONG(1.00)");
    CHECK(describeTargets({{"X", -1.0}}) == "SHORT(1.00)");
    CHECK(describeTargets({{"A", 0.0}, {"B", 0.0}}) == "FLAT");
    CHECK(describeTargets({{"A", 0.45}, {"B", -0.55}}) == "A:+0.45 B:-0.55");

    SignalEvent e;
    e.barOpenTime = 1791309600000LL;  // 2026-10-06 18:00Z
    e.universe = "BTCUSDT";
    e.strategy = "Momentum";
    e.close = 86595.61;
    e.weights = {{"BTCUSDT", 1.0}};
    e.previous = "FLAT";
    e.changed = true;
    std::ostringstream os;
    ConsolePrinter(os).onSignal(e);
    CHECK(os.str() ==
          "2026-10-06 18:00Z BTCUSDT close=86595.6100 strategy=Momentum target=LONG(1.00) [changed from FLAT]\n");
    e.changed = false;
    e.recommended = false;
    std::ostringstream os2;
    ConsolePrinter(os2).onSignal(e);
    CHECK(os2.str().find("NOT RECOMMENDED") != std::string::npos);
    CHECK(os2.str().find("changed from") == std::string::npos);
}

TEST(file_logger_writes_header_once_and_rows) {
    const std::string path = "logs/_test_live_signals.csv";
    std::remove(path.c_str());
    SignalEvent e;
    e.barOpenTime = 1791309600000LL;
    e.universe = "ETHUSDT/BTCUSDT";
    e.strategy = "PairsTrading";
    e.close = 2722.24;
    e.weights = {{"ETHUSDT", 0.5}, {"BTCUSDT", -0.5}};
    e.changed = true;
    FileLogger log(path);
    log.onSignal(e);
    log.onSignal(e);
    std::ifstream in(path);
    std::string l0, l1, l2, l3;
    std::getline(in, l0);
    std::getline(in, l1);
    std::getline(in, l2);
    CHECK(!std::getline(in, l3));
    CHECK(l0 == "timestamp,universe,strategy,close,target_weights,changed");
    CHECK(l1 == "2026-10-06 18:00Z,ETHUSDT/BTCUSDT,PairsTrading,2722.24000000,ETHUSDT=0.5000;BTCUSDT=-0.5000,1");
    CHECK(l1 == l2);
    std::remove(path.c_str());
}

TEST(live_runner_seed_poll_and_changed_flag) {
    auto src = momentumWorld(380);  // still rising: momentum is long
    LiveRunner runner(src, choice("AAAUSDT", "Momentum", "fast=5;slow=20"), "1h", 300, 30);
    runner.setClock([&] { return src->nowMs(); });
    auto sink = std::make_shared<Collector>();
    runner.attach(sink);

    runner.seed();
    runner.emitCurrent();
    CHECK(sink->events.size() == 1);
    CHECK(sink->events[0].weights[0].weight == 1.0);  // long during the uptrend
    CHECK(!sink->events[0].changed);
    CHECK(runner.lastBarOpenTime() == src->fetchLatest("AAAUSDT", "1h").openTime);

    CHECK(!runner.pollOnce());  // nothing new has closed yet
    CHECK(sink->events.size() == 1);

    // Keep the world moving until the downtrend flips the signal to FLAT.
    bool flipped = false;
    int64_t previousOpen = runner.lastBarOpenTime();
    for (int i = 0; i < 400 && !flipped; ++i) {
        src->advance(1);
        CHECK(runner.pollOnce());
        const SignalEvent& e = sink->events.back();
        CHECK(e.barOpenTime == previousOpen + 3600000LL);  // bars arrive in order, one at a time
        previousOpen = e.barOpenTime;
        if (e.changed) {
            flipped = true;
            CHECK(e.previous == "LONG(1.00)");
            CHECK(e.weights[0].weight == 0.0);
        }
    }
    CHECK(flipped);
}

TEST(live_runner_catches_up_after_an_outage) {
    auto src = momentumWorld(380);
    LiveRunner runner(src, choice("AAAUSDT", "Momentum", "fast=5;slow=20"), "1h", 300, 30);
    runner.setClock([&] { return src->nowMs(); });
    auto sink = std::make_shared<Collector>();
    runner.attach(sink);
    runner.seed();
    const int64_t before = runner.lastBarOpenTime();
    src->advance(4);  // four candles closed while we were away
    CHECK(runner.pollOnce());
    CHECK(sink->events.size() == 4);
    for (size_t i = 0; i < 4; ++i) CHECK(sink->events[i].barOpenTime == before + static_cast<int64_t>(i + 1) * 3600000LL);
    CHECK(runner.lastBarOpenTime() == before + 4 * 3600000LL);
}

TEST(live_runner_retries_with_backoff_and_keeps_running) {
    Logger::setConsoleEnabled(false);
    auto src = momentumWorld(380);
    LiveRunner runner(src, choice("AAAUSDT", "Momentum", "fast=5;slow=20"), "1h", 300, 1);
    runner.setClock([&] { return src->nowMs(); });
    auto sink = std::make_shared<Collector>();
    runner.attach(sink);

    // The sleeper plays the part of "time passing": on the first sleep a candle closes and the
    // network starts failing. Total slept time proves the 1 s and 2 s back-offs happened.
    long totalMs = 0;
    bool armed = false;
    runner.setSleeper([&](int ms) {
        totalMs += ms;
        if (!armed) {
            armed = true;
            src->advance(1);
            src->failuresLeft = 2;
        }
        if (sink->events.size() >= 2) LiveRunner::requestStop();  // stop once the new bar was delivered
    });
    LiveRunner::clearStop();
    runner.run(false);  // must not throw despite NetworkError
    LiveRunner::clearStop();
    CHECK(sink->events.size() == 2);  // the seed signal + the bar that arrived after the outage
    CHECK(totalMs >= 1000 + 1000 + 1000 + 2000);  // 3 poll waits of 1 s + back-offs of 1 s and 2 s
    CHECK(src->failuresLeft == 0);
}

TEST(live_runner_once_mode_emits_one_signal_and_returns) {
    auto src = momentumWorld(380);
    LiveRunner runner(src, choice("AAAUSDT", "Momentum", "fast=5;slow=20"), "1h", 300, 30);
    runner.setClock([&] { return src->nowMs(); });
    auto sink = std::make_shared<Collector>();
    runner.attach(sink);
    runner.run(true);
    CHECK(sink->events.size() == 1);
    CHECK(src->latestCalls >= 1);
}

TEST(live_runner_works_for_a_pair_universe) {
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<double> a, b;
    double lnB = std::log(100.0), u = 0;
    for (int i = 0; i < 1500; ++i) {
        lnB += 0.01 * nd(rng);
        u = 0.9 * u + 0.012 * nd(rng);
        b.push_back(std::exp(lnB));
        a.push_back(std::exp(0.5 + 1.2 * lnB + u));
    }
    auto src = std::make_shared<FakeDataSource>(
        std::vector<CandleSeries>{seriesFromCloses("AAAUSDT", a), seriesFromCloses("BBBUSDT", b)}, 1200);
    LiveRunner runner(src, choice("AAAUSDT/BBBUSDT", "PairsTrading", "window=60;entryZ=2;exitZ=0;stopZ=4;hedge=0"), "1h",
                      1000, 30);
    runner.setClock([&] { return src->nowMs(); });
    auto sink = std::make_shared<Collector>();
    runner.attach(sink);
    runner.seed();
    CHECK(runner.currentTargets().size() == 2);
    src->advance(5);
    CHECK(runner.pollOnce());
    CHECK(sink->events.size() == 5);
    CHECK(sink->events.back().universe == "AAAUSDT/BBBUSDT");
}

TEST(live_runner_needs_enough_history) {
    auto src = momentumWorld(100);  // only 100 closed bars, train window wants 300
    LiveRunner runner(src, choice("AAAUSDT", "Momentum", "fast=5;slow=20"), "1h", 300, 30);
    runner.setClock([&] { return src->nowMs(); });
    CHECK_THROWS(runner.seed(), DataError);
    CHECK_THROWS(runner.pollOnce(), ConfigError);  // not seeded
}
