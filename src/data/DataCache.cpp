#include "quant/data/DataCache.hpp"

#include "quant/core/Errors.hpp"
#include "quant/core/FileUtil.hpp"
#include "quant/core/Logger.hpp"
#include "quant/core/TimeUtil.hpp"

namespace quant {

DataCache::DataCache(std::string folder, std::shared_ptr<IDataSource> remote)
    : folder_(folder), csv_(folder), remote_(std::move(remote)) {}

bool DataCache::has(const std::string& symbol, const std::string& interval) const {
    return csv_.exists(symbol, interval);
}

CandleSeries DataCache::load(const std::string& symbol, const std::string& interval) const {
    if (!has(symbol, interval))
        throw DataError("no cached data for " + symbol + " " + interval + " in '" + folder_ +
                        "'; run option 1 (download) first");
    size_t gaps = 0;
    int64_t firstGap = -1;
    CandleSeries s = CsvDataSource::readFile(csv_.pathFor(symbol, interval), symbol, interval, &gaps,
                                             &firstGap);
    if (gaps > 0)
        Logger::warn(symbol + " " + interval + ": " + std::to_string(gaps) +
                     " gap(s) in cached data, first at " + formatUtc(firstGap));
    return s;
}

CandleSeries DataCache::update(const std::string& symbol, const std::string& interval, int days) {
    if (!remote_) throw DataError("DataCache::update needs a remote data source");
    const int64_t step = intervalToMs(interval);
    const int64_t now = nowMs();
    ensureDirectory(folder_);

    CandleSeries existing(symbol, interval);
    const int64_t wanted = now - static_cast<int64_t>(days) * 86400000LL;
    int64_t from = wanted;
    if (has(symbol, interval)) {
        existing = load(symbol, interval);
        // A cache that starts later than the requested history cannot be extended backwards by
        // appending, so it is rebuilt from scratch.
        if (!existing.empty() && existing.front().openTime > wanted + 2 * step) {
            Logger::info(symbol + ": cached history is shorter than requested, re-downloading");
            existing = CandleSeries(symbol, interval);
        } else if (!existing.empty()) {
            from = existing.back().openTime + step;
        }
    }

    CandleSeries fresh = remote_->fetchHistory(symbol, interval, from, now);
    for (const auto& c : fresh) {
        // Guard against the remote re-sending a bar we already have.
        if (existing.empty() || c.openTime > existing.back().openTime) existing.push_back(c);
    }
    CsvDataSource::writeFile(csv_.pathFor(symbol, interval), existing);
    return existing;
}

}  // namespace quant
