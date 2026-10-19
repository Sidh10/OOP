#include "quant/data/CsvDataSource.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"

namespace quant {

namespace {
const char* kHeader = "open_time,open,high,low,close,volume,close_time";

[[noreturn]] void bad(const std::string& path, size_t row, const std::string& why) {
    throw DataError("corrupt CSV " + path + " row " + std::to_string(row) + ": " + why);
}

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::string cell;
    std::istringstream ss(line);
    while (std::getline(ss, cell, ',')) out.push_back(cell);
    if (!line.empty() && line.back() == ',') out.push_back("");
    return out;
}

double toDouble(const std::string& s, const std::string& path, size_t row, const char* col) {
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size() || !std::isfinite(d))
        bad(path, row, std::string("cannot parse ") + col + " from '" + s + "'");
    return d;
}

int64_t toInt(const std::string& s, const std::string& path, size_t row, const char* col) {
    char* end = nullptr;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (s.empty() || end != s.c_str() + s.size())
        bad(path, row, std::string("cannot parse ") + col + " from '" + s + "'");
    return v;
}
}  // namespace

std::string CsvDataSource::pathFor(const std::string& symbol, const std::string& interval) const {
    return folder_ + "/" + symbol + "_" + interval + ".csv";
}

bool CsvDataSource::exists(const std::string& symbol, const std::string& interval) const {
    return std::ifstream(pathFor(symbol, interval)).good();
}

CandleSeries CsvDataSource::readFile(const std::string& path, const std::string& symbol,
                                     const std::string& interval, size_t* gapCount,
                                     int64_t* firstGapMs) {
    std::ifstream in(path);
    if (!in) throw DataError("cannot open CSV file " + path);
    std::string line;
    if (!std::getline(in, line)) throw DataError("CSV file " + path + " is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kHeader) bad(path, 1, "unexpected header '" + line + "'");

    const int64_t step = intervalToMs(interval);
    std::vector<Candle> bars;
    size_t gaps = 0;
    int64_t firstGap = -1;
    size_t row = 1;  // header is row 1; first data row is row 2
    while (std::getline(in, line)) {
        ++row;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) bad(path, row, "empty line");
        auto f = splitCsv(line);
        if (f.size() != 7) bad(path, row, "expected 7 columns, got " + std::to_string(f.size()));
        Candle c;
        c.openTime = toInt(f[0], path, row, "open_time");
        c.open = toDouble(f[1], path, row, "open");
        c.high = toDouble(f[2], path, row, "high");
        c.low = toDouble(f[3], path, row, "low");
        c.close = toDouble(f[4], path, row, "close");
        c.volume = toDouble(f[5], path, row, "volume");
        c.closeTime = toInt(f[6], path, row, "close_time");
        if (c.open <= 0 || c.high <= 0 || c.low <= 0 || c.close <= 0)
            bad(path, row, "prices must be > 0");
        if (c.high < std::max(c.open, c.close) || c.low > std::min(c.open, c.close))
            bad(path, row, "high/low inconsistent with open/close");
        if (!bars.empty()) {
            if (c.openTime <= bars.back().openTime)
                bad(path, row, "open_time not strictly increasing");
            if (c.openTime - bars.back().openTime != step) {
                if (gaps == 0) firstGap = c.openTime;
                ++gaps;
            }
        }
        bars.push_back(c);
    }
    if (gapCount) *gapCount = gaps;
    if (firstGapMs) *firstGapMs = firstGap;
    return CandleSeries(symbol, interval, std::move(bars));
}

void CsvDataSource::writeFile(const std::string& path, const CandleSeries& series) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw DataError("cannot write CSV file " + path);
    out << kHeader << "\n";
    char buf[256];
    for (const auto& c : series) {
        std::snprintf(buf, sizeof buf, "%lld,%.8f,%.8f,%.8f,%.8f,%.8f,%lld\n",
                      static_cast<long long>(c.openTime), c.open, c.high, c.low, c.close, c.volume,
                      static_cast<long long>(c.closeTime));
        out << buf;
    }
    if (!out) throw DataError("write failed for CSV file " + path);
}

CandleSeries CsvDataSource::fetchHistory(const std::string& symbol, const std::string& interval,
                                         int64_t fromMs, int64_t toMs) {
    CandleSeries all = readFile(pathFor(symbol, interval), symbol, interval);
    std::vector<Candle> kept;
    for (const auto& c : all)
        if (c.openTime >= fromMs && c.openTime <= toMs) kept.push_back(c);
    return CandleSeries(symbol, interval, std::move(kept));
}

Candle CsvDataSource::fetchLatest(const std::string& symbol, const std::string& interval) {
    CandleSeries all = readFile(pathFor(symbol, interval), symbol, interval);
    if (all.empty()) throw DataError("CSV for " + symbol + " has no candles");
    return all.back();
}

}  // namespace quant
