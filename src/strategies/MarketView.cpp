#include "quant/strategies/MarketView.hpp"

#include "quant/core/Errors.hpp"

namespace quant {

MarketView::MarketView(const std::vector<CandleSeries>& data, size_t begin, size_t index)
    : data_(&data), begin_(begin), index_(index) {
    if (data.empty()) throw DataError("MarketView needs at least one series");
    if (begin > index || index >= data[0].size())
        throw DataError("MarketView range out of bounds");
}

const Candle& MarketView::barAgo(size_t slot, size_t ago) const {
    if (ago >= count()) throw DataError("MarketView::barAgo reaches before the visible window");
    return (*data_)[slot][index_ - ago];
}

CandleSeries MarketView::history(size_t slot, size_t n) const {
    const size_t take = n < count() ? n : count();
    return (*data_)[slot].slice(index_ + 1 - take, index_ + 1);
}

size_t MarketView::slotOf(const std::string& symbol) const {
    for (size_t i = 0; i < data_->size(); ++i)
        if ((*data_)[i].symbol() == symbol) return i;
    throw DataError("symbol " + symbol + " is not in this MarketView");
}

}  // namespace quant
