#include "quant/engine/PerformanceMetrics.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "quant/core/TimeUtil.hpp"

namespace quant {

double PerformanceMetrics::periodsPerYear(const std::string& interval) {
    return 365.0 * 86400000.0 / static_cast<double>(intervalToMs(interval));
}

std::vector<double> PerformanceMetrics::returnsOf(const std::vector<double>& equity) {
    std::vector<double> r;
    if (equity.size() < 2) return r;
    r.reserve(equity.size() - 1);
    for (size_t i = 1; i < equity.size(); ++i)
        r.push_back(equity[i - 1] > 0 ? equity[i] / equity[i - 1] - 1.0 : 0.0);
    return r;
}

std::vector<double> PerformanceMetrics::equityFromReturns(const std::vector<double>& returns, double start) {
    std::vector<double> e;
    e.reserve(returns.size() + 1);
    e.push_back(start);
    for (double r : returns) e.push_back(e.back() * (1.0 + r));
    return e;
}

PerformanceMetrics::PerformanceMetrics(const std::vector<double>& equity, double ppy) {
    nBars_ = equity.size();
    if (equity.size() < 2) return;
    const std::vector<double> r = returnsOf(equity);
    const double n = static_cast<double>(r.size());

    const double mean = std::accumulate(r.begin(), r.end(), 0.0) / n;
    double ss = 0, down = 0;
    for (double x : r) {
        ss += (x - mean) * (x - mean);
        down += std::min(x, 0.0) * std::min(x, 0.0);
    }
    const double sd = r.size() > 1 ? std::sqrt(ss / (n - 1.0)) : 0.0;
    const double dd = std::sqrt(down / n);
    const double ann = std::sqrt(ppy);
    sharpe_ = sd < 1e-12 ? 0.0 : mean / sd * ann;
    sortino_ = dd < 1e-12 ? 0.0 : mean / dd * ann;

    double peak = equity[0], worst = 0;
    for (double e : equity) {
        peak = std::max(peak, e);
        if (peak > 0) worst = std::max(worst, (peak - e) / peak);
    }
    maxDD_ = worst;

    totalReturn_ = equity.front() > 0 ? equity.back() / equity.front() - 1.0 : 0.0;
    if (equity.front() > 0 && equity.back() > 0)
        cagr_ = std::pow(equity.back() / equity.front(), ppy / static_cast<double>(equity.size())) - 1.0;
    else
        cagr_ = -1.0;
}

void PerformanceMetrics::setTradeStats(int numTrades, double winRate, double exposure, double turnover) {
    numTrades_ = numTrades;
    winRate_ = winRate;
    exposure_ = exposure;
    turnover_ = turnover;
}

}  // namespace quant
