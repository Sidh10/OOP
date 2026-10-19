#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "quant/core/Candle.hpp"

namespace quant {

class CandleSeries {
public:
    CandleSeries() = default;
    CandleSeries(std::string symbol, std::string interval, std::vector<Candle> bars = {});

    const std::string& symbol() const { return symbol_; }
    const std::string& interval() const { return interval_; }
    size_t size() const { return bars_.size(); }
    bool empty() const { return bars_.empty(); }

    // operator[] is unchecked (hot loops); at() throws DataError.
    const Candle& operator[](size_t i) const { return bars_[i]; }
    Candle& operator[](size_t i) { return bars_[i]; }
    const Candle& at(size_t i) const;
    const Candle& front() const { return bars_.front(); }
    const Candle& back() const { return bars_.back(); }

    void push_back(const Candle& c) { bars_.push_back(c); }

    // Half-open [from, to), clamped to the series size.
    CandleSeries slice(size_t from, size_t to) const;

    std::vector<Candle>::const_iterator begin() const { return bars_.begin(); }
    std::vector<Candle>::const_iterator end() const { return bars_.end(); }

private:
    std::string symbol_;
    std::string interval_;
    std::vector<Candle> bars_;
};

std::ostream& operator<<(std::ostream& os, const CandleSeries& s);

// Inner join on openTime so every series ends up with identical timestamps.
// Returns the number of bars dropped (summed over all series).
size_t alignSeries(std::vector<CandleSeries>& series);

}  // namespace quant
