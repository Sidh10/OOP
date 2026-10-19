#include "quant/core/CandleSeries.hpp"

#include <algorithm>
#include <iterator>
#include <ostream>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"

namespace quant {

std::ostream& operator<<(std::ostream& os, const Candle& c) {
    os << formatUtc(c.openTime) << " O=" << c.open << " H=" << c.high << " L=" << c.low
       << " C=" << c.close << " V=" << c.volume;
    return os;
}

CandleSeries::CandleSeries(std::string symbol, std::string interval, std::vector<Candle> bars)
    : symbol_(std::move(symbol)), interval_(std::move(interval)), bars_(std::move(bars)) {}

const Candle& CandleSeries::at(size_t i) const {
    if (i >= bars_.size())
        throw DataError("CandleSeries::at: index " + std::to_string(i) + " out of range (size " +
                        std::to_string(bars_.size()) + ") for " + symbol_);
    return bars_[i];
}

CandleSeries CandleSeries::slice(size_t from, size_t to) const {
    to = std::min(to, bars_.size());
    from = std::min(from, to);
    return CandleSeries(symbol_, interval_,
                        std::vector<Candle>(bars_.begin() + static_cast<std::ptrdiff_t>(from),
                                            bars_.begin() + static_cast<std::ptrdiff_t>(to)));
}

std::ostream& operator<<(std::ostream& os, const CandleSeries& s) {
    os << s.symbol() << " " << s.interval() << " " << s.size() << " bars";
    if (!s.empty())
        os << " [" << formatUtc(s[0].openTime) << " .. " << formatUtc(s.back().openTime) << "]";
    return os;
}

size_t alignSeries(std::vector<CandleSeries>& series) {
    if (series.empty()) return 0;
    // Intersect timestamps pairwise; every series is sorted ascending.
    std::vector<int64_t> common;
    for (const auto& c : series[0]) common.push_back(c.openTime);
    for (size_t k = 1; k < series.size(); ++k) {
        std::vector<int64_t> next, other;
        for (const auto& c : series[k]) other.push_back(c.openTime);
        std::set_intersection(common.begin(), common.end(), other.begin(), other.end(),
                              std::back_inserter(next));
        common.swap(next);
    }
    size_t dropped = 0;
    for (auto& s : series) {
        std::vector<Candle> kept;
        kept.reserve(common.size());
        size_t j = 0;
        for (const auto& c : s) {
            while (j < common.size() && common[j] < c.openTime) ++j;
            if (j < common.size() && common[j] == c.openTime) kept.push_back(c);
        }
        dropped += s.size() - kept.size();
        s = CandleSeries(s.symbol(), s.interval(), std::move(kept));
    }
    return dropped;
}

}  // namespace quant
