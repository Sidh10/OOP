#include "quant/engine/Portfolio.hpp"

#include <cmath>

namespace quant {

double Portfolio::equity(const std::vector<double>& prices) const {
    double e = cash_;
    for (size_t i = 0; i < units_.size(); ++i) e += units_[i] * prices[i];
    return e;
}

double Portfolio::grossValue(const std::vector<double>& prices) const {
    double g = 0;
    for (size_t i = 0; i < units_.size(); ++i) g += std::fabs(units_[i] * prices[i]);
    return g;
}

bool Portfolio::isFlat() const {
    for (double u : units_)
        if (u != 0.0) return false;
    return true;
}

std::vector<Fill> Portfolio::rebalance(const std::vector<double>& weights,
                                       const std::vector<bool>& trade,
                                       const std::vector<double>& openPrices, const CostModel& cost) {
    std::vector<Fill> fills;
    const double eq = equity(openPrices);
    if (eq <= 0) return fills;

    double gross = 0;
    for (double w : weights) gross += std::fabs(w);
    const double scale = gross > 1.0 ? 1.0 / gross : 1.0;

    const size_t n = units_.size();
    std::vector<double> dUnits(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        if (!trade[i]) continue;
        // A zero weight closes the position exactly (no floating point residue).
        const double target = weights[i] == 0.0 ? 0.0 : weights[i] * scale * eq / openPrices[i];
        dUnits[i] = target - units_[i];
    }

    const double slip = cost.slippageRate();
    // 1) sells (and short sales) first: they raise cash
    for (size_t i = 0; i < n; ++i) {
        if (dUnits[i] >= 0.0) continue;
        const double notional = -dUnits[i] * openPrices[i];
        const double fee = cost.fee(notional);
        const double price = openPrices[i] * (1.0 - slip);
        cash_ += -dUnits[i] * price - fee;
        units_[i] += dUnits[i];  // target 0 gives dUnits == -units exactly, so the result is exactly 0
        fills.push_back({i, dUnits[i], price, notional, fee});
    }
    // 2) buys, scaled so that sum(dV * (1 + fee + slip)) <= cash
    double buyCost = 0;
    for (size_t i = 0; i < n; ++i)
        if (dUnits[i] > 0.0) buyCost += dUnits[i] * openPrices[i] * (1.0 + cost.feeRate() + slip);
    double f = 1.0;
    if (buyCost > cash_) f = cash_ > 0.0 ? cash_ / buyCost : 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (dUnits[i] <= 0.0 || f <= 0.0) continue;
        const double du = dUnits[i] * f;
        const double notional = du * openPrices[i];
        const double fee = cost.fee(notional);
        const double price = openPrices[i] * (1.0 + slip);
        cash_ -= du * price + fee;
        units_[i] += du;
        fills.push_back({i, du, price, notional, fee});
    }
    if (cash_ < 0.0 && cash_ > -1e-9) cash_ = 0.0;  // rounding dust only; real overdrafts cannot occur
    return fills;
}

}  // namespace quant
