#include "quant/strategies/StrategyFactory.hpp"

#include <functional>

#include "quant/core/Errors.hpp"
#include "quant/strategies/AdvancedStrategies.hpp"
#include "quant/strategies/SimpleStrategies.hpp"

namespace quant {

namespace {

struct Entry {
    std::string name;
    std::function<std::unique_ptr<Strategy>()> make;
    std::function<ParameterGrid()> grid;
    bool pair;
};

const std::vector<Entry>& table() {
    static const std::vector<Entry> t = {
        {"BuyAndHold", [] { return std::make_unique<BuyAndHoldStrategy>(); },
         [] { return ParameterGrid(); }, false},
        {"MeanReversion", [] { return std::make_unique<MeanReversionStrategy>(); },
         [] {
             ParameterGrid g;
             g.axis("window", {20, 50, 100}).axis("entryZ", {1.5, 2.0, 2.5}).axis("exitZ", {0.0, 0.5});
             return g;
         },
         false},
        {"Momentum", [] { return std::make_unique<MomentumStrategy>(); },
         [] {
             ParameterGrid g;
             g.axis("fast", {5, 10, 20, 50}).axis("slow", {30, 50, 100, 200});
             g.require([](const ParamSet& p) { return p.get("fast") < p.get("slow"); });
             return g;
         },
         false},
        {"Breakout", [] { return std::make_unique<BreakoutStrategy>(); },
         [] {
             ParameterGrid g;
             g.axis("N", {20, 50, 100}).axis("M", {10, 20, 50});
             g.require([](const ParamSet& p) { return p.get("M") < p.get("N"); });
             return g;
         },
         false},
        {"PairsTrading", [] { return std::make_unique<PairsTradingStrategy>(); },
         [] {
             ParameterGrid g;
             g.axis("window", {60, 120, 240})
                 .axis("entryZ", {1.5, 2.0, 2.5})
                 .axis("exitZ", {0.0, 0.5})
                 .axis("stopZ", {4.0})
                 .axis("hedge", {0, 1});  // 0 = rolling OLS, 1 = Kalman
             return g;
         },
         true},
        {"MLSignal", [] { return std::make_unique<MLSignalStrategy>(); },
         [] {
             ParameterGrid g;
             g.axis("lr", {0.01, 0.05})
                 .axis("l2", {0.0, 0.001})
                 .axis("threshold", {0.52, 0.55, 0.6})
                 .axis("online", {0, 1});
             return g;
         },
         false},
    };
    return t;
}

const Entry& find(const std::string& name) {
    for (const auto& e : table())
        if (e.name == name) return e;
    throw ConfigError("unknown strategy '" + name + "'");
}

}  // namespace

std::unique_ptr<Strategy> StrategyFactory::create(const std::string& name) { return find(name).make(); }

std::vector<std::string> StrategyFactory::registered() {
    std::vector<std::string> out;
    for (const auto& e : table()) out.push_back(e.name);
    return out;
}

std::vector<std::string> StrategyFactory::candidates() {
    std::vector<std::string> out;
    for (const auto& e : table())
        if (e.name != "BuyAndHold") out.push_back(e.name);
    return out;
}

ParameterGrid StrategyFactory::grid(const std::string& name) { return find(name).grid(); }

bool StrategyFactory::isPairStrategy(const std::string& name) { return find(name).pair; }

}  // namespace quant
