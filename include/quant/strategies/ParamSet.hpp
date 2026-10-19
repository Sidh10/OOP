#pragma once

#include <map>
#include <string>

namespace quant {

// Named numeric parameters, e.g. {window=50; entryZ=2}. Ordered map so printing is stable.
class ParamSet {
public:
    ParamSet() = default;
    ParamSet(std::initializer_list<std::pair<const std::string, double>> init) : v_(init) {}

    void set(const std::string& key, double value) { v_[key] = value; }
    bool has(const std::string& key) const { return v_.count(key) > 0; }
    double get(const std::string& key) const;                 // throws ConfigError if missing
    double get(const std::string& key, double fallback) const;
    void merge(const ParamSet& other);                        // other's values win
    const std::map<std::string, double>& values() const { return v_; }

    std::string toString() const;                             // "k=v;k=v"
    static ParamSet fromString(const std::string& text);      // inverse; "" gives an empty set

    bool operator==(const ParamSet& o) const { return v_ == o.v_; }

private:
    std::map<std::string, double> v_;
};

}  // namespace quant
