#pragma once

#include <vector>

namespace quant {

// Common interface for the small statistical / learning models: feed (x, y) pairs one at a time
// with update(), or all at once with fit(); then ask predict(x). Strategies own models by
// composition and use them without knowing the concrete type.
class Model {
public:
    virtual ~Model() = default;
    virtual void reset() = 0;
    virtual void update(const std::vector<double>& x, double y) = 0;
    virtual double predict(const std::vector<double>& x) const = 0;
    // Default: start from scratch and replay the pairs. Models with a better batch method override it.
    virtual void fit(const std::vector<std::vector<double>>& X, const std::vector<double>& y);
};

}  // namespace quant
