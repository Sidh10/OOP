#pragma once

#include <string>
#include <vector>

namespace quant {

// Return-based statistics of one equity curve (SPEC section 7). Risk-free rate is 0.
class PerformanceMetrics {
public:
    PerformanceMetrics() = default;
    PerformanceMetrics(const std::vector<double>& equity, double periodsPerYear);

    // Trade-level numbers come from the Backtester, not from the curve.
    void setTradeStats(int numTrades, double winRate, double exposure, double turnover);

    double sharpe() const { return sharpe_; }
    double sortino() const { return sortino_; }
    double maxDrawdown() const { return maxDD_; }
    double cagr() const { return cagr_; }
    double totalReturn() const { return totalReturn_; }
    int numTrades() const { return numTrades_; }
    double winRate() const { return winRate_; }
    double exposure() const { return exposure_; }
    double turnover() const { return turnover_; }
    size_t numBars() const { return nBars_; }

    static double periodsPerYear(const std::string& interval);  // crypto trades 24/7
    static std::vector<double> returnsOf(const std::vector<double>& equity);
    // Compounds a return series into an equity curve that starts at `start`.
    static std::vector<double> equityFromReturns(const std::vector<double>& returns, double start = 1.0);

private:
    double sharpe_ = 0, sortino_ = 0, maxDD_ = 0, cagr_ = 0, totalReturn_ = 0;
    int numTrades_ = 0;
    double winRate_ = 0, exposure_ = 0, turnover_ = 0;
    size_t nBars_ = 0;
};

}  // namespace quant
