#pragma once

#include <string>

#include "quant/data/IDataSource.hpp"

namespace quant {

// Reads and writes the cache format from SPEC section 2:
//   data/{SYMBOL}_{interval}.csv, header open_time,open,high,low,close,volume,close_time
class CsvDataSource : public IDataSource {
public:
    explicit CsvDataSource(std::string folder) : folder_(std::move(folder)) {}

    CandleSeries fetchHistory(const std::string& symbol, const std::string& interval,
                              int64_t fromMs, int64_t toMs) override;
    Candle fetchLatest(const std::string& symbol, const std::string& interval) override;

    std::string pathFor(const std::string& symbol, const std::string& interval) const;
    bool exists(const std::string& symbol, const std::string& interval) const;

    // Whole-file operations. readFile validates and throws DataError naming file and row.
    // Gaps (diff != interval) are counted and returned through gapCount/firstGapMs, not fatal.
    static CandleSeries readFile(const std::string& path, const std::string& symbol,
                                 const std::string& interval, size_t* gapCount = nullptr,
                                 int64_t* firstGapMs = nullptr);
    static void writeFile(const std::string& path, const CandleSeries& series);

private:
    std::string folder_;
};

}  // namespace quant
