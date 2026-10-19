#include "quant/data/BinanceDataSource.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <thread>

#include <nlohmann/json.hpp>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"

namespace quant {

namespace {
constexpr int kPageLimit = 1000;
constexpr int kPageSleepMs = 250;  // klines weight is 2 per request; stay far below the limit
constexpr int kMaxTries = 5;

void validateSymbol(const std::string& s) {
    bool ok = !s.empty();
    for (char c : s) ok = ok && std::isalnum(static_cast<unsigned char>(c));
    if (!ok) throw ConfigError("invalid symbol '" + s + "'");
}

// Strict string -> double: the whole string must be a number.
double parseNumber(const nlohmann::json& v, const char* what, size_t row) {
    if (!v.is_string())
        throw ParseError(std::string("kline row ") + std::to_string(row) + ": " + what +
                         " is not a string");
    const std::string s = v.get<std::string>();
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size())
        throw ParseError(std::string("kline row ") + std::to_string(row) + ": bad number '" + s +
                         "' for " + what);
    return d;
}
}  // namespace

BinanceDataSource::BinanceDataSource(std::shared_ptr<IHttpClient> http,
                                     std::vector<std::string> baseUrls)
    : http_(std::move(http)),
      bases_(std::move(baseUrls)),
      sleeper_([](int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }),
      clock_(nowMs) {
    if (!http_) throw ConfigError("BinanceDataSource needs an HTTP client");
    if (bases_.empty()) throw ConfigError("BinanceDataSource needs at least one base URL");
}

std::vector<Candle> BinanceDataSource::parseKlines(const std::string& json) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception& e) {
        throw ParseError(std::string("kline JSON is not valid: ") + e.what());
    }
    if (doc.is_object()) {  // Binance error payloads look like {"code":-1121,"msg":"Invalid symbol."}
        std::string msg = doc.value("msg", std::string("unexpected JSON object"));
        throw ParseError("Binance error: " + msg);
    }
    if (!doc.is_array()) throw ParseError("kline response is not a JSON array");

    std::vector<Candle> out;
    out.reserve(doc.size());
    size_t row = 0;
    for (const auto& r : doc) {
        if (!r.is_array() || r.size() != 12)
            throw ParseError("kline row " + std::to_string(row) + " is not an array of 12 fields");
        if (!r[0].is_number_integer() || !r[6].is_number_integer())
            throw ParseError("kline row " + std::to_string(row) + ": times must be integers");
        Candle c;
        c.openTime = r[0].get<int64_t>();
        c.open = parseNumber(r[1], "open", row);
        c.high = parseNumber(r[2], "high", row);
        c.low = parseNumber(r[3], "low", row);
        c.close = parseNumber(r[4], "close", row);
        c.volume = parseNumber(r[5], "volume", row);
        c.closeTime = r[6].get<int64_t>();
        out.push_back(c);
        ++row;
    }
    return out;
}

std::string BinanceDataSource::buildUrl(const std::string& base, const std::string& symbol,
                                        const std::string& interval, int64_t startMs,
                                        int64_t endMs, int limit) {
    std::string url = base + "/api/v3/klines?symbol=" + symbol + "&interval=" + interval;
    if (startMs >= 0) url += "&startTime=" + std::to_string(startMs);
    if (endMs >= 0) url += "&endTime=" + std::to_string(endMs);
    url += "&limit=" + std::to_string(limit);
    return url;
}

std::string BinanceDataSource::request(const std::string& pathAndQuery) {
    std::string lastError;
    for (size_t tried = 0; tried < bases_.size(); ++tried) {
        const size_t idx = (activeBase_ + tried) % bases_.size();
        int attempt = 0;
        while (true) {
            try {
                std::string body = http_->get(bases_[idx] + pathAndQuery);
                activeBase_ = idx;  // remember the base that works
                return body;
            } catch (const NetworkError& e) {
                const int st = e.httpStatus();
                if (st == 418)  // IP ban: retrying makes it worse
                    throw NetworkError("Binance returned HTTP 418 (IP banned); stopping", 418);
                if (st == 429) {
                    if (++attempt >= kMaxTries)
                        throw NetworkError("Binance rate limit (HTTP 429) persisted after " +
                                               std::to_string(kMaxTries) + " tries",
                                           429);
                    int waitSec = e.retryAfterSec() >= 0 ? e.retryAfterSec() : (1 << (attempt - 1));
                    sleeper_(waitSec * 1000);
                    continue;
                }
                if (st == 0 || st == 403 || st == 451) {  // unreachable or geo-blocked: try the next base
                    lastError = e.what();
                    break;
                }
                throw;
            }
        }
    }
    throw NetworkError("all Binance base URLs failed; last error: " + lastError);
}

CandleSeries BinanceDataSource::fetchHistory(const std::string& symbol, const std::string& interval,
                                             int64_t fromMs, int64_t toMs) {
    validateSymbol(symbol);
    const int64_t step = intervalToMs(interval);
    const int64_t now = clock_();

    std::vector<Candle> all;
    int64_t cur = fromMs;
    bool first = true;
    while (cur <= toMs) {
        if (!first) sleeper_(kPageSleepMs);
        first = false;
        std::string body = request(buildUrl("", symbol, interval, cur, toMs, kPageLimit));
        std::vector<Candle> page = parseKlines(body);
        if (page.empty()) break;
        all.insert(all.end(), page.begin(), page.end());
        const int64_t next = page.back().openTime + step;
        if (static_cast<int>(page.size()) < kPageLimit || next <= cur) break;
        cur = next;
    }

    // Drop the still-forming candle, then sort and dedupe by open time.
    all.erase(std::remove_if(all.begin(), all.end(),
                             [now](const Candle& c) { return c.closeTime >= now; }),
              all.end());
    std::sort(all.begin(), all.end(),
              [](const Candle& a, const Candle& b) { return a.openTime < b.openTime; });
    all.erase(std::unique(all.begin(), all.end(),
                          [](const Candle& a, const Candle& b) { return a.openTime == b.openTime; }),
              all.end());
    return CandleSeries(symbol, interval, std::move(all));
}

Candle BinanceDataSource::fetchLatest(const std::string& symbol, const std::string& interval) {
    validateSymbol(symbol);
    intervalToMs(interval);  // validates
    const int64_t now = clock_();
    std::string body = request(buildUrl("", symbol, interval, -1, -1, 2));
    std::vector<Candle> rows = parseKlines(body);
    for (auto it = rows.rbegin(); it != rows.rend(); ++it)
        if (it->closeTime < now) return *it;
    throw DataError("no closed candle returned for " + symbol + " " + interval);
}

}  // namespace quant
