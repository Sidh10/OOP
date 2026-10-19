#pragma once

#include <string>
#include <utility>
#include <vector>

#include "quant/Defaults.hpp"
#include "quant/research/WalkForwardOptimizer.hpp"

namespace quant {

// Runtime settings: starts from Defaults.hpp, optionally overridden by config.txt.
// Verdict thresholds are deliberately NOT configurable (D-011).
struct Config {
    std::vector<std::string> symbols = defaults::symbols;
    std::vector<std::pair<std::string, std::string>> pairs = defaults::pairs;
    std::vector<std::string> baseUrls = defaults::baseUrls;
    std::string interval = defaults::interval;
    int historyDays = defaults::historyDays;
    double initialEquity = defaults::initialEquity;
    double feeBps = defaults::feeBps;
    double slippageBps = defaults::slippageBps;
    size_t wfTrain = defaults::wfTrainBars;
    size_t wfTest = defaults::wfTestBars;
    size_t wfStep = defaults::wfStepBars;
    int minTradesTrain = defaults::minTradesTrain;
    int minTradesOos = defaults::minTradesOos;
    int pollSeconds = defaults::livePollSeconds;
    std::string dataDir = defaults::dataDir;
    std::string reportDir = defaults::reportDir;
    std::string logDir = defaults::logDir;

    // Missing file is fine (defaults). A malformed line throws ConfigError naming file and line.
    static Config load(const std::string& path);
    // Same parser on text, so tests do not need a file. `source` is only used in error messages.
    static Config fromText(const std::string& text, const std::string& source = "config");

    WalkForwardConfig walkForward() const;
};

}  // namespace quant
