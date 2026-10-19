#pragma once

#include <string>
#include <vector>

#include "quant/core/CandleSeries.hpp"

namespace quant {

// A read-only window onto aligned series that ends at bar `index` (inclusive) and starts at
// `begin`. Strategies only ever get a MarketView, so they structurally cannot read bars after
// the current one: this is the main defence against lookahead bias.
class MarketView {
public:
    MarketView(const std::vector<CandleSeries>& data, size_t begin, size_t index);

    size_t numSymbols() const { return data_->size(); }
    const std::string& symbol(size_t slot) const { return (*data_)[slot].symbol(); }

    size_t count() const { return index_ - begin_ + 1; }  // visible bars
    size_t index() const { return index_; }               // absolute index of the current bar

    const Candle& bar(size_t slot) const { return (*data_)[slot][index_]; }
    const Candle& bar(const std::string& symbol) const { return bar(slotOf(symbol)); }
    // `ago` = 0 is the current bar. Throws DataError beyond the visible start.
    const Candle& barAgo(size_t slot, size_t ago) const;
    // Last n visible bars (fewer if less history), oldest first.
    CandleSeries history(size_t slot, size_t n) const;
    CandleSeries history(const std::string& symbol, size_t n) const { return history(slotOf(symbol), n); }

    size_t slotOf(const std::string& symbol) const;

private:
    const std::vector<CandleSeries>* data_;
    size_t begin_;
    size_t index_;
};

}  // namespace quant
