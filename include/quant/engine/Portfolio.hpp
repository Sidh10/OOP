#pragma once

#include <vector>

#include "quant/engine/CostModel.hpp"

namespace quant {

struct Fill {
    size_t slot = 0;       // which asset (index into the Backtester's series list)
    double units = 0;      // signed: + bought, - sold
    double price = 0;      // actual fill price including slippage
    double notional = 0;   // |units| * open price (the dV of SPEC section 7)
    double fee = 0;
};

// The only object that mutates cash and positions (encapsulation): everything else asks for a
// rebalance and reads the results. Asset-agnostic: assets are slots 0..n-1.
class Portfolio {
public:
    Portfolio(double cash, size_t numAssets) : cash_(cash), units_(numAssets, 0.0) {}

    double cash() const { return cash_; }
    double units(size_t slot) const { return units_[slot]; }
    double equity(const std::vector<double>& prices) const;
    double grossValue(const std::vector<double>& prices) const;  // sum |units * price|
    bool isFlat() const;                                         // every position exactly zero

    // Moves the slots with trade[i]==true to weight[i] * equity, marked at the open prices.
    // Sells execute first, buys are scaled down so cash never goes negative (spot constraint).
    // Gross exposure above 1 is scaled down. Fill price = open * (1 +/- slippage).
    std::vector<Fill> rebalance(const std::vector<double>& weights, const std::vector<bool>& trade,
                                const std::vector<double>& openPrices, const CostModel& cost);

private:
    double cash_;
    std::vector<double> units_;
};

}  // namespace quant
