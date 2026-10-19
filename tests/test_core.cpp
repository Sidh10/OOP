#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

#include "minitest.hpp"
#include "quant/core/CandleSeries.hpp"
#include "quant/core/Errors.hpp"
#include "quant/core/Logger.hpp"
#include "quant/core/RollingWindow.hpp"
#include "quant/core/TimeUtil.hpp"

using namespace quant;

namespace {
Candle bar(int64_t t, double px) {
    Candle c;
    c.openTime = t;
    c.closeTime = t + 3599999;
    c.open = c.high = c.low = c.close = px;
    c.volume = 1;
    return c;
}
CandleSeries makeSeries(const std::string& sym, std::initializer_list<int64_t> times) {
    CandleSeries s(sym, "1h");
    double px = 100;
    for (auto t : times) s.push_back(bar(t * 3600000LL, px += 1));
    return s;
}
}  // namespace

TEST(series_basics) {
    CandleSeries s = makeSeries("X", {0, 1, 2, 3, 4});
    CHECK(s.size() == 5);
    CHECK(s[2].openTime == 2 * 3600000LL);
    CHECK_NEAR(s.at(4).close, 105, 0);
    CHECK_THROWS(s.at(5), DataError);
    CandleSeries sub = s.slice(1, 3);
    CHECK(sub.size() == 2);
    CHECK(sub[0].openTime == 3600000LL);
    CHECK(s.slice(3, 99).size() == 2);  // clamped
    CHECK(s.slice(4, 2).size() == 0);   // inverted range is empty, not UB
    double sum = 0;
    for (const auto& c : s) sum += c.close;
    CHECK_NEAR(sum, 101 + 102 + 103 + 104 + 105, 1e-12);
    std::ostringstream os;
    os << s;
    CHECK(os.str().find("X 1h 5 bars") == 0);
}

TEST(align_series_inner_join) {
    std::vector<CandleSeries> v{makeSeries("A", {0, 1, 2, 4, 5}), makeSeries("B", {1, 2, 3, 5, 6})};
    size_t dropped = alignSeries(v);
    CHECK(v[0].size() == 3 && v[1].size() == 3);  // common: 1, 2, 5
    CHECK(dropped == 4);
    for (size_t i = 0; i < 3; ++i) CHECK(v[0][i].openTime == v[1][i].openTime);
    CHECK(v[0][2].openTime == 5 * 3600000LL);
}

TEST(rolling_window_matches_naive_after_100k_pushes) {
    const size_t W = 50;
    RollingWindow<double> w(W);
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(1000.0, 5.0);  // large mean, small spread stresses stability
    std::vector<double> all;
    for (int i = 0; i < 100000; ++i) {
        double x = nd(rng);
        all.push_back(x);
        w.push(x);
    }
    double mean = 0;
    for (size_t i = all.size() - W; i < all.size(); ++i) mean += all[i];
    mean /= W;
    double var = 0;
    for (size_t i = all.size() - W; i < all.size(); ++i) var += (all[i] - mean) * (all[i] - mean);
    var /= (W - 1);
    CHECK(std::fabs(w.mean() - mean) <= 1e-9 * std::fabs(mean));
    CHECK(std::fabs(w.variance() - var) <= 1e-9 * var);
    CHECK(w.full());
    CHECK_NEAR(w.at(0), all[all.size() - W], 0);
    CHECK_NEAR(w.newest(), all.back(), 0);
}

TEST(rolling_window_partial_fill) {
    RollingWindow<double> w(3);
    w.push(1);
    w.push(2);
    CHECK(w.size() == 2 && !w.full());
    CHECK_NEAR(w.mean(), 1.5, 1e-12);
    CHECK_NEAR(w.variance(), 0.5, 1e-12);
    w.push(3);
    w.push(4);  // window is now 2,3,4
    CHECK_NEAR(w.mean(), 3, 1e-12);
    CHECK_NEAR(w.variance(), 1, 1e-12);
    CHECK_THROWS(RollingWindow<double>(0), DataError);
}

TEST(time_helpers) {
    CHECK(formatUtc(1791309600000LL) == "2026-10-06 18:00Z");
    CHECK(formatUtcFull(951868799000LL) == "2000-02-29 23:59:59Z");  // leap day
    CHECK(timestampForFilename(1791309600000LL) == "20261006_180000");
    CHECK(intervalToMs("1h") == 3600000LL);
    CHECK(intervalToMs("1d") == 86400000LL);
    CHECK_THROWS(intervalToMs("7x"), ConfigError);
}

TEST(error_hierarchy) {
    CHECK_THROWS(throw NetworkError("x", 429, 3), QuantError);
    CHECK_THROWS(throw ParseError("x"), QuantError);
    CHECK_THROWS(throw DataError("x"), std::runtime_error);
    try {
        throw NetworkError("slow down", 429, 7);
    } catch (const NetworkError& e) {
        CHECK(e.httpStatus() == 429 && e.retryAfterSec() == 7);
    }
}

TEST(logger_writes_levels_to_file) {
    const std::string path = "logs/_test_engine.log";
    std::remove(path.c_str());
    Logger::setConsoleEnabled(false);
    Logger::setLogFile(path);
    Logger::info("hello info");
    Logger::warn("hello warn");
    Logger::error("hello error");
    Logger::setLogFile("");
    std::ifstream in(path);
    std::string a, b, c;
    std::getline(in, a);
    std::getline(in, b);
    std::getline(in, c);
    CHECK(a.find("[INFO] hello info") != std::string::npos);
    CHECK(b.find("[WARN] hello warn") != std::string::npos);
    CHECK(c.find("[ERROR] hello error") != std::string::npos);
    CHECK(a.size() > 20 && a[4] == '-' && a.find("Z [") != std::string::npos);  // UTC timestamp prefix
    in.close();
    std::remove(path.c_str());
}
