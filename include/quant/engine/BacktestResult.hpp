#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "quant/engine/PerformanceMetrics.hpp"
#include "quant/engine/Portfolio.hpp"

namespace quant {

// A flat -> exposed -> flat episode.
struct TradeEpisode {
    size_t entryBar = 0;  // absolute bar index of the entry fill
    size_t exitBar = 0;   // absolute bar index of the exit fill (or the last bar if still open)
    double pnl = 0;       // net of fees and slippage
    bool closed = false;
};

struct BarFill {
    size_t bar = 0;  // absolute bar index at whose OPEN the fill happened
    Fill fill;
};

class BacktestResult {
public:
    std::string strategyName;
    std::string params;
    size_t firstBar = 0;                 // absolute index of equity[0]
    std::vector<double> equity;          // marked at each bar's close
    std::vector<double> returns;         // equity[t]/equity[t-1] - 1
    std::vector<std::vector<double>> targets;  // decision made at the close of bar firstBar+i (per slot)
    std::vector<BarFill> fills;
    std::vector<TradeEpisode> trades;
    PerformanceMetrics metrics;
};

std::ostream& operator<<(std::ostream& os, const BacktestResult& r);

}  // namespace quant
