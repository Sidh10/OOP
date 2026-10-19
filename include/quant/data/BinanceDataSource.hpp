#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "quant/Defaults.hpp"
#include "quant/data/IDataSource.hpp"
#include "quant/data/IHttpClient.hpp"

namespace quant {

class BinanceDataSource : public IDataSource {
public:
    explicit BinanceDataSource(std::shared_ptr<IHttpClient> http,
                               std::vector<std::string> baseUrls = defaults::baseUrls);

    CandleSeries fetchHistory(const std::string& symbol, const std::string& interval,
                              int64_t fromMs, int64_t toMs) override;
    Candle fetchLatest(const std::string& symbol, const std::string& interval) override;

    // Injection points so tests run instantly and deterministically.
    void setSleeper(std::function<void(int ms)> sleeper) { sleeper_ = std::move(sleeper); }
    void setClock(std::function<int64_t()> nowMsFn) { clock_ = std::move(nowMsFn); }

    // Pure parsing step, public so it can be unit-tested on fixtures.
    static std::vector<Candle> parseKlines(const std::string& json);

    static std::string buildUrl(const std::string& base, const std::string& symbol,
                                const std::string& interval, int64_t startMs, int64_t endMs,
                                int limit);

private:
    // Tries base URLs in order (falling through on connection failure, 403, 451) and applies
    // the 429 back-off. `pathAndQuery` starts with "/api/v3/klines?...".
    std::string request(const std::string& pathAndQuery);

    std::shared_ptr<IHttpClient> http_;
    std::vector<std::string> bases_;
    size_t activeBase_ = 0;
    std::function<void(int)> sleeper_;
    std::function<int64_t()> clock_;
};

}  // namespace quant
