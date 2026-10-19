#include "quant/research/ParameterGrid.hpp"

namespace quant {

ParameterGrid& ParameterGrid::axis(const std::string& key, std::vector<double> values) {
    axes_.emplace_back(key, std::move(values));
    return *this;
}

ParameterGrid& ParameterGrid::require(Constraint c) {
    constraint_ = std::move(c);
    return *this;
}

std::vector<ParamSet> ParameterGrid::combinations() const {
    std::vector<ParamSet> out;
    if (axes_.empty()) return out;
    std::vector<size_t> idx(axes_.size(), 0);
    while (true) {
        ParamSet p;
        for (size_t a = 0; a < axes_.size(); ++a) p.set(axes_[a].first, axes_[a].second[idx[a]]);
        if (!constraint_ || constraint_(p)) out.push_back(p);
        // odometer-style increment, last axis fastest
        size_t a = axes_.size();
        while (a > 0) {
            --a;
            if (++idx[a] < axes_[a].second.size()) break;
            idx[a] = 0;
            if (a == 0) return out;
        }
    }
}

}  // namespace quant
