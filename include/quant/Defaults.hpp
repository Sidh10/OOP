#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Single place for every default (SPEC section 0). config.txt can override the
// tunable ones; the verdict thresholds are fixed up front and are not
// meant to be tuned after looking at out-of-sample results.
namespace quant::defaults {

inline const std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "BNBUSDT", "SOLUSDT"};
// {dependent A, regressor B}
inline const std::vector<std::pair<std::string, std::string>> pairs = {
    {"ETHUSDT", "BTCUSDT"}, {"SOLUSDT", "ETHUSDT"}, {"BNBUSDT", "ETHUSDT"}};
inline const std::vector<std::string> baseUrls = {"https://api.binance.com",
                                                  "https://data-api.binance.vision",
                                                  "https://api1.binance.com"};

inline const std::string interval = "1h";
constexpr int historyDays = 730;
constexpr double initialEquity = 10000.0;
constexpr double feeBps = 10.0;       // per side
constexpr double slippageBps = 2.0;   // per side

constexpr size_t wfTrainBars = 2000;
constexpr size_t wfTestBars = 500;
constexpr size_t wfStepBars = 500;
constexpr int minTradesTrain = 10;
constexpr int minTradesOos = 20;

// Verdict thresholds (SPEC section 8).
constexpr double recommendSharpeMargin = 0.2;  // must beat buy-and-hold Sharpe by this much
constexpr double recommendMinSharpe = 0.5;
constexpr double recommendMaxOverfitGap = 1.0;

constexpr int livePollSeconds = 30;
constexpr unsigned randomSeed = 42;

inline const std::string dataDir = "data";
inline const std::string reportDir = "reports";
inline const std::string logDir = "logs";

}  // namespace quant::defaults
