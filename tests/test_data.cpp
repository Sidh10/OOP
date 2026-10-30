#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>

#include "minitest.hpp"
#include "quant/core/Errors.hpp"
#include "quant/core/Logger.hpp"
#include "quant/core/TimeUtil.hpp"
#include "quant/data/BinanceDataSource.hpp"
#include "quant/data/CsvDataSource.hpp"
#include "quant/data/DataCache.hpp"

using namespace quant;

namespace {

constexpr int64_t H = 3600000;

std::string readAll(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open fixture " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Builds a synthetic kline JSON page: n rows starting at openTime t0, 1h apart.
std::string syntheticPage(int64_t t0, int n) {
    std::ostringstream os;
    os << "[";
    for (int i = 0; i < n; ++i) {
        int64_t t = t0 + i * H;
        if (i) os << ",";
        os << "[" << t << ",\"100.0\",\"101.0\",\"99.0\",\"100.5\",\"1.5\"," << (t + H - 1)
           << ",\"0\",1,\"0\",\"0\",\"0\"]";
    }
    os << "]";
    return os.str();
}

struct Rig {
    std::shared_ptr<MockHttpClient> http = std::make_shared<MockHttpClient>();
    BinanceDataSource src;
    std::vector<int> sleeps;
    Rig() : src(http, {"https://a.test", "https://b.test"}) {
        src.setClock([] { return 4000000LL * H; });  // "now" is far in the future of all test data
        src.setSleeper([this](int ms) { sleeps.push_back(ms); });
    }
};

}  // namespace

TEST(kline_parsing_fixture) {
    auto rows = BinanceDataSource::parseKlines(readAll("tests/fixtures/klines_sample.json"));
    CHECK(rows.size() == 3);
    CHECK(rows[0].openTime == 1700000000000LL);
    CHECK(rows[0].closeTime == 1700003599999LL);
    CHECK_NEAR(rows[0].open, 100.5, 1e-12);
    CHECK_NEAR(rows[0].high, 105.25, 1e-12);
    CHECK_NEAR(rows[0].low, 99.75, 1e-12);
    CHECK_NEAR(rows[0].close, 104.0, 1e-12);
    CHECK_NEAR(rows[0].volume, 12.345, 1e-12);
    CHECK_NEAR(rows[2].close, 102.0, 1e-12);
    CHECK_NEAR(rows[2].volume, 20.125, 1e-12);
}

TEST(kline_parsing_rejects_malformed) {
    // too few fields
    CHECK_THROWS(BinanceDataSource::parseKlines("[[1,\"1\",\"1\",\"1\",\"1\",\"1\",2]]"), ParseError);
    // price is a number instead of a string
    CHECK_THROWS(BinanceDataSource::parseKlines(
                     "[[1,1.0,\"1\",\"1\",\"1\",\"1\",2,\"0\",1,\"0\",\"0\",\"0\"]]"),
                 ParseError);
    // garbage number
    CHECK_THROWS(BinanceDataSource::parseKlines(
                     "[[1,\"abc\",\"1\",\"1\",\"1\",\"1\",2,\"0\",1,\"0\",\"0\",\"0\"]]"),
                 ParseError);
    // not JSON at all, and a Binance error object
    CHECK_THROWS(BinanceDataSource::parseKlines("<html>"), ParseError);
    CHECK_THROWS(BinanceDataSource::parseKlines("{\"code\":-1121,\"msg\":\"Invalid symbol.\"}"), ParseError);
}

TEST(paging_1000_then_37) {
    Rig r;
    const int64_t t0 = 1000 * H;
    r.http->queueResponse(syntheticPage(t0, 1000));
    r.http->queueResponse(syntheticPage(t0 + 1000 * H, 37));
    CandleSeries s = r.src.fetchHistory("BTCUSDT", "1h", t0, t0 + 5000 * H);
    CHECK(s.size() == 1037);
    for (size_t i = 1; i < s.size(); ++i) CHECK(s[i].openTime == s[i - 1].openTime + H);
    CHECK(r.http->requests().size() == 2);
    CHECK(r.http->requests()[0].find("startTime=" + std::to_string(t0)) != std::string::npos);
    CHECK(r.http->requests()[1].find("startTime=" + std::to_string(t0 + 1000 * H)) != std::string::npos);
    CHECK(r.http->requests()[0].find("limit=1000") != std::string::npos);
    CHECK(r.http->requests()[0].find("/api/v3/klines?symbol=BTCUSDT&interval=1h") != std::string::npos);
    CHECK(r.sleeps.size() == 1 && r.sleeps[0] == 250);  // one pause between the two pages
}

TEST(unclosed_candle_dropped) {
    Rig r;
    r.src.setClock([] { return 1000LL * H + 2 * H + H / 2; });  // inside the third bar
    r.http->queueResponse(syntheticPage(1000 * H, 3));          // third bar closes at 1003H-1 > now
    CandleSeries s = r.src.fetchHistory("BTCUSDT", "1h", 1000 * H, 2000 * H);
    CHECK(s.size() == 2);
    CHECK(s.back().openTime == 1001 * H);
}

TEST(history_dedupes_and_sorts) {
    Rig r;
    // Rows arrive out of order with one duplicate: t+1, t, t+1.
    auto one = [](int64_t t) { return syntheticPage(t, 1).substr(1, syntheticPage(t, 1).size() - 2); };
    r.http->queueResponse("[" + one(1001 * H) + "," + one(1000 * H) + "," + one(1001 * H) + "]");
    CandleSeries s = r.src.fetchHistory("ETHUSDT", "1h", 1000 * H, 1010 * H);
    CHECK(s.size() == 2);
    CHECK(s[0].openTime == 1000 * H && s[1].openTime == 1001 * H);
}

TEST(latest_returns_last_closed) {
    Rig r;
    r.src.setClock([] { return 1000LL * H + H + H / 2; });  // bar 1001 is still forming
    r.http->queueResponse(syntheticPage(1000 * H, 2));
    Candle c = r.src.fetchLatest("BTCUSDT", "1h");
    CHECK(c.openTime == 1000 * H);
    CHECK(r.http->requests()[0].find("limit=2") != std::string::npos);
    CHECK(r.http->requests()[0].find("startTime") == std::string::npos);
}

TEST(base_url_fallback_on_451) {
    Rig r;
    r.http->queueError(451);
    r.http->queueResponse(syntheticPage(1000 * H, 2));
    CandleSeries s = r.src.fetchHistory("BTCUSDT", "1h", 1000 * H, 1010 * H);
    CHECK(s.size() == 2);
    CHECK(r.http->requests()[0].find("https://a.test") == 0);
    CHECK(r.http->requests()[1].find("https://b.test") == 0);
}

TEST(all_bases_failing_throws_network_error) {
    Rig r;
    r.http->queueError(451);
    r.http->queueError(403);
    CHECK_THROWS(r.src.fetchHistory("BTCUSDT", "1h", 1000 * H, 1010 * H), NetworkError);
}

TEST(rate_limit_429_backs_off_then_succeeds) {
    Rig r;
    r.http->queueError(429);     // no Retry-After: 1 s
    r.http->queueError(429, 7);  // Retry-After: 7 s
    r.http->queueResponse(syntheticPage(1000 * H, 2));
    CandleSeries s = r.src.fetchHistory("BTCUSDT", "1h", 1000 * H, 1010 * H);
    CHECK(s.size() == 2);
    CHECK(r.sleeps.size() == 2);
    CHECK(r.sleeps[0] == 1000);
    CHECK(r.sleeps[1] == 7000);
}

TEST(ban_418_stops_immediately) {
    Rig r;
    r.http->queueError(418);
    CHECK_THROWS(r.src.fetchHistory("BTCUSDT", "1h", 1000 * H, 1010 * H), NetworkError);
    CHECK(r.http->requests().size() == 1);
}

TEST(bad_symbol_rejected_before_request) {
    Rig r;
    CHECK_THROWS(r.src.fetchHistory("BTC&x=1", "1h", 0, 1), ConfigError);
    CHECK(r.http->requests().empty());
}

TEST(csv_round_trip_and_incremental_append) {
    Logger::setConsoleEnabled(false);
    const std::string dir = "logs";  // any existing writable folder
    CsvDataSource csv(dir);
    std::string path = csv.pathFor("TESTCSV", "1h");

    auto rows = BinanceDataSource::parseKlines(readAll("tests/fixtures/klines_sample.json"));
    CandleSeries s("TESTCSV", "1h", rows);
    CsvDataSource::writeFile(path, s);
    CandleSeries back = CsvDataSource::readFile(path, "TESTCSV", "1h");
    CHECK(back.size() == 3);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(back[i].openTime == s[i].openTime);
        CHECK(back[i].closeTime == s[i].closeTime);
        CHECK_NEAR(back[i].open, s[i].open, 1e-9);
        CHECK_NEAR(back[i].close, s[i].close, 1e-9);
        CHECK_NEAR(back[i].volume, s[i].volume, 1e-9);
    }

    // Incremental: DataCache fetches only bars after the last cached one and appends.
    auto http = std::make_shared<MockHttpClient>();
    auto bsrc = std::make_shared<BinanceDataSource>(http, std::vector<std::string>{"https://a.test"});
    bsrc->setClock([] { return 2000000000000LL; });
    bsrc->setSleeper([](int) {});
    DataCache cache(dir, bsrc);
    const int64_t last = s.back().openTime;
    http->queueResponse(syntheticPage(last + H, 2));
    CandleSeries upd = cache.update("TESTCSV", "1h", 30);
    CHECK(upd.size() == 5);
    CHECK(http->requests().size() == 1);
    CHECK(http->requests()[0].find("startTime=" + std::to_string(last + H)) != std::string::npos);
    CandleSeries reread = cache.load("TESTCSV", "1h");
    CHECK(reread.size() == 5);
    CHECK(reread.back().openTime == last + 2 * H);
    std::remove(path.c_str());
}

TEST(csv_corrupt_file_names_file_and_row) {
    const std::string path = "logs/CORRUPT_1h.csv";
    {
        std::ofstream f(path);
        f << "open_time,open,high,low,close,volume,close_time\n";
        f << "3600000,1.0,2.0,0.5,1.5,10.0,7199999\n";
        f << "7200000,1.0,abc,0.5,1.5,10.0,10799999\n";
    }
    try {
        CsvDataSource::readFile(path, "CORRUPT", "1h");
        CHECK(false);  // must have thrown
    } catch (const DataError& e) {
        std::string msg = e.what();
        CHECK(msg.find("CORRUPT_1h.csv") != std::string::npos);
        CHECK(msg.find("row 3") != std::string::npos);
    }
    {
        std::ofstream f(path);  // out-of-order timestamps
        f << "open_time,open,high,low,close,volume,close_time\n";
        f << "7200000,1.0,2.0,0.5,1.5,10.0,10799999\n";
        f << "3600000,1.0,2.0,0.5,1.5,10.0,7199999\n";
    }
    CHECK_THROWS(CsvDataSource::readFile(path, "CORRUPT", "1h"), DataError);
    {
        std::ofstream f(path);  // high below close
        f << "open_time,open,high,low,close,volume,close_time\n";
        f << "3600000,1.0,1.2,0.5,1.5,10.0,7199999\n";
    }
    CHECK_THROWS(CsvDataSource::readFile(path, "CORRUPT", "1h"), DataError);
    std::remove(path.c_str());
    CHECK_THROWS(CsvDataSource::readFile("logs/does_not_exist.csv", "X", "1h"), DataError);
}

TEST(csv_gap_is_counted_not_fatal) {
    const std::string path = "logs/GAP_1h.csv";
    {
        std::ofstream f(path);
        f << "open_time,open,high,low,close,volume,close_time\n";
        f << "3600000,1.0,2.0,0.5,1.5,10.0,7199999\n";
        f << "10800000,1.0,2.0,0.5,1.5,10.0,14399999\n";  // one bar missing
    }
    size_t gaps = 0;
    int64_t first = -1;
    CandleSeries s = CsvDataSource::readFile(path, "GAP", "1h", &gaps, &first);
    CHECK(s.size() == 2);
    CHECK(gaps == 1);
    CHECK(first == 10800000LL);
    std::remove(path.c_str());
}

// Regression: a 5-day cache used to stay 5 days long after asking for 730, because update() only
// appended after the last cached bar. Now a cache that starts later than the requested history
// is rebuilt from the start of the requested range.
TEST(datacache_rebuilds_cache_that_does_not_cover_requested_history) {
    Logger::setConsoleEnabled(false);
    const int64_t now = nowMs();
    const int64_t recentStart = (now / H) * H - 5 * H;  // cache holds only the last 5 hours
    CandleSeries shortCache("COVERUSDT", "1h");
    for (int i = 0; i < 5; ++i) {
        Candle c;
        c.openTime = recentStart + i * H;
        c.closeTime = c.openTime + H - 1;
        c.open = c.high = c.low = c.close = 100.0;
        c.volume = 1;
        shortCache.push_back(c);
    }
    const std::string path = "logs/COVERUSDT_1h.csv";
    CsvDataSource::writeFile(path, shortCache);

    auto http = std::make_shared<MockHttpClient>();
    auto bsrc = std::make_shared<BinanceDataSource>(http, std::vector<std::string>{"https://a.test"});
    bsrc->setClock([] { return 4000000LL * H; });
    bsrc->setSleeper([](int) {});
    DataCache cache("logs", bsrc);

    const int days = 30;
    const int64_t wantedStart = now - static_cast<int64_t>(days) * 86400000LL;
    const int64_t firstBar = (wantedStart / H) * H;
    http->queueResponse(syntheticPage(firstBar, 40));  // what the exchange returns for the full range
    CandleSeries out = cache.update("COVERUSDT", "1h", days);

    // It asked for the whole requested range, not just "after the last cached bar".
    CHECK(http->requests().size() == 1);
    const std::string& url = http->requests()[0];
    const size_t at = url.find("startTime=");
    CHECK(at != std::string::npos);
    const int64_t asked = std::atoll(url.c_str() + at + 10);
    CHECK(std::llabs(asked - wantedStart) < 60000);       // within a minute of now - 30 days
    CHECK(asked < recentStart);                            // and certainly not last cached bar + 1h
    CHECK(out.size() == 40);                               // rebuilt from the downloaded range
    CHECK(out.front().openTime == firstBar);
    CHECK(cache.load("COVERUSDT", "1h").front().openTime == firstBar);  // and saved that way
    std::remove(path.c_str());
}
