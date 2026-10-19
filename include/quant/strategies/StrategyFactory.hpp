#pragma once

#include <memory>
#include <string>
#include <vector>

#include "quant/research/ParameterGrid.hpp"
#include "quant/strategies/Strategy.hpp"

namespace quant {

// Factory pattern: strategies are created by name, so research code and the menu never
// depend on concrete strategy classes. Also knows each strategy's parameter grid.
class StrategyFactory {
public:
    static std::unique_ptr<Strategy> create(const std::string& name);  // throws ConfigError if unknown
    static std::vector<std::string> registered();                      // every name, incl. BuyAndHold
    static std::vector<std::string> candidates();                      // registered() minus the baseline
    static ParameterGrid grid(const std::string& name);                // empty grid for BuyAndHold
    static bool isPairStrategy(const std::string& name);               // needs two symbols (A, B)
};

}  // namespace quant
