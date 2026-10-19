#pragma once

#include <memory>
#include <string>

#include "quant/data/CsvDataSource.hpp"
#include "quant/data/IDataSource.hpp"

namespace quant {

// Keeps data/{SYMBOL}_{interval}.csv up to date from a remote IDataSource.
class DataCache {
public:
    DataCache(std::string folder, std::shared_ptr<IDataSource> remote);

    bool has(const std::string& symbol, const std::string& interval) const;
    // Loads and validates the cached file (logs a warning if it has gaps).
    CandleSeries load(const std::string& symbol, const std::string& interval) const;
    // Cold start: download `days` of history. Warm: fetch only bars after the last cached one.
    // Returns the full updated series.
    CandleSeries update(const std::string& symbol, const std::string& interval, int days);

private:
    std::string folder_;
    CsvDataSource csv_;
    std::shared_ptr<IDataSource> remote_;
};

}  // namespace quant
