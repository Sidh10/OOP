#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "quant/strategies/Strategy.hpp"

namespace quant {

struct SignalEvent {
    int64_t barOpenTime = 0;  // ms, open time of the closed candle the signal is based on
    std::string universe;     // "BTCUSDT" or "ETHUSDT/BTCUSDT"
    std::string strategy;
    double close = 0;         // close of the first symbol of the universe
    std::vector<TargetWeight> weights;
    std::string previous;     // description of the previous target, "" if none
    bool changed = false;
    bool recommended = true;  // false = shadow mode, strategy did not earn a RECOMMENDED verdict
};

// Observer pattern: LiveRunner publishes signals, any number of listeners react.
class ISignalListener {
public:
    virtual ~ISignalListener() = default;
    virtual void onSignal(const SignalEvent& e) = 0;
};

// "FLAT", "LONG(1.00)", "SHORT(1.00)" for one symbol; "FLAT" or "ETHUSDT:+0.45 BTCUSDT:-0.55" for several.
std::string describeTargets(const std::vector<TargetWeight>& w);

class ConsolePrinter : public ISignalListener {
public:
    explicit ConsolePrinter(std::ostream& os) : os_(os) {}
    void onSignal(const SignalEvent& e) override;

private:
    std::ostream& os_;
};

// Appends to logs/live_signals.csv: timestamp,universe,strategy,close,target_weights,changed
class FileLogger : public ISignalListener {
public:
    explicit FileLogger(std::string path) : path_(std::move(path)) {}
    void onSignal(const SignalEvent& e) override;

private:
    std::string path_;
};

}  // namespace quant
