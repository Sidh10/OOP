#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "quant/strategies/ParamSet.hpp"

namespace quant {

// Cartesian product of per-parameter value lists, optionally filtered (e.g. fast < slow).
class ParameterGrid {
public:
    using Constraint = std::function<bool(const ParamSet&)>;

    ParameterGrid& axis(const std::string& key, std::vector<double> values);
    ParameterGrid& require(Constraint c);

    std::vector<ParamSet> combinations() const;
    bool empty() const { return axes_.empty(); }

private:
    std::vector<std::pair<std::string, std::vector<double>>> axes_;
    Constraint constraint_;
};

}  // namespace quant
