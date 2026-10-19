#include "quant/strategies/ParamSet.hpp"

#include <cstdlib>
#include <sstream>

#include "quant/core/Errors.hpp"

namespace quant {

double ParamSet::get(const std::string& key) const {
    auto it = v_.find(key);
    if (it == v_.end()) throw ConfigError("missing parameter '" + key + "'");
    return it->second;
}

double ParamSet::get(const std::string& key, double fallback) const {
    auto it = v_.find(key);
    return it == v_.end() ? fallback : it->second;
}

void ParamSet::merge(const ParamSet& other) {
    for (const auto& kv : other.v_) v_[kv.first] = kv.second;
}

std::string ParamSet::toString() const {
    std::ostringstream os;
    os.precision(12);
    bool first = true;
    for (const auto& kv : v_) {
        if (!first) os << ";";
        first = false;
        os << kv.first << "=" << kv.second;
    }
    return os.str();
}

ParamSet ParamSet::fromString(const std::string& text) {
    ParamSet p;
    std::istringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (item.empty()) continue;
        auto eq = item.find('=');
        if (eq == std::string::npos) throw ParseError("bad parameter item '" + item + "'");
        const std::string val = item.substr(eq + 1);
        char* end = nullptr;
        double d = std::strtod(val.c_str(), &end);
        if (val.empty() || end != val.c_str() + val.size())
            throw ParseError("bad parameter value in '" + item + "'");
        p.set(item.substr(0, eq), d);
    }
    return p;
}

}  // namespace quant
