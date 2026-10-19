#pragma once

#include <cstdint>
#include <string>

#include "quant/core/CandleSeries.hpp"

namespace quant {

// Anything that can produce candles: the Binance REST API, or a CSV file on disk.
class IDataSource {
public:
    virtual ~IDataSource() = default;
    // Closed candles with openTime in [fromMs, toMs], ascending, no duplicates.
    virtual CandleSeries fetchHistory(const std::string& symbol, const std::string& interval,
                                      int64_t fromMs, int64_t toMs) = 0;
    // Most recent closed candle.
    virtual Candle fetchLatest(const std::string& symbol, const std::string& interval) = 0;
};

}  // namespace quant
