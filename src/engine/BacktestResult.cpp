#include "quant/engine/BacktestResult.hpp"

#include <iomanip>
#include <ostream>

namespace quant {

std::ostream& operator<<(std::ostream& os, const BacktestResult& r) {
    const PerformanceMetrics& m = r.metrics;
    os << std::fixed << std::setprecision(3);
    os << r.strategyName;
    if (!r.params.empty()) os << " [" << r.params << "]";
    os << "\n  bars=" << m.numBars() << "  return=" << m.totalReturn() * 100 << "%"
       << "  CAGR=" << m.cagr() * 100 << "%"
       << "  Sharpe=" << m.sharpe() << "  Sortino=" << m.sortino() << "\n  maxDD=" << m.maxDrawdown() * 100
       << "%  trades=" << m.numTrades() << "  winRate=" << m.winRate() * 100 << "%"
       << "  exposure=" << m.exposure() * 100 << "%  turnover=" << m.turnover() << "x";
    return os;
}

}  // namespace quant
