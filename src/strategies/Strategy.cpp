#include "quant/strategies/Strategy.hpp"

namespace quant {

void Strategy::setParams(const ParamSet& p) {
    params_.merge(p);
    applyParams();
    reset();
}

}  // namespace quant
