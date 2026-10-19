#include "quant/Config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "quant/core/Errors.hpp"

namespace quant {

namespace {
std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::vector<std::string> splitList(const std::string& v) {
    std::vector<std::string> out;
    std::istringstream ss(v);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

[[noreturn]] void fail(const std::string& src, int line, const std::string& why) {
    throw ConfigError(src + " line " + std::to_string(line) + ": " + why);
}

double number(const std::string& v, const std::string& src, int line) {
    char* end = nullptr;
    double d = std::strtod(v.c_str(), &end);
    if (v.empty() || end != v.c_str() + v.size()) fail(src, line, "'" + v + "' is not a number");
    return d;
}
}  // namespace

Config Config::fromText(const std::string& text, const std::string& source) {
    Config c;
    std::istringstream in(text);
    std::string raw;
    int line = 0;
    while (std::getline(in, raw)) {
        ++line;
        const auto hash = raw.find('#');
        const std::string s = trim(hash == std::string::npos ? raw : raw.substr(0, hash));
        if (s.empty()) continue;
        const auto eq = s.find('=');
        if (eq == std::string::npos) fail(source, line, "expected key=value, got '" + s + "'");
        const std::string key = trim(s.substr(0, eq));
        const std::string val = trim(s.substr(eq + 1));
        if (val.empty()) fail(source, line, "empty value for '" + key + "'");

        if (key == "symbols") {
            c.symbols = splitList(val);
            if (c.symbols.empty()) fail(source, line, "symbols list is empty");
        } else if (key == "pairs") {  // ETHUSDT/BTCUSDT,SOLUSDT/ETHUSDT
            c.pairs.clear();
            for (const auto& p : splitList(val)) {
                const auto slash = p.find('/');
                if (slash == std::string::npos || slash == 0 || slash + 1 == p.size())
                    fail(source, line, "pair '" + p + "' must look like A/B");
                c.pairs.emplace_back(p.substr(0, slash), p.substr(slash + 1));
            }
        } else if (key == "base_urls") {
            c.baseUrls = splitList(val);
            if (c.baseUrls.empty()) fail(source, line, "base_urls list is empty");
        } else if (key == "interval") {
            c.interval = val;
        } else if (key == "history_days") {
            c.historyDays = static_cast<int>(number(val, source, line));
            if (c.historyDays <= 0) fail(source, line, "history_days must be > 0");
        } else if (key == "initial_equity") {
            c.initialEquity = number(val, source, line);
            if (c.initialEquity <= 0) fail(source, line, "initial_equity must be > 0");
        } else if (key == "fee_bps") {
            c.feeBps = number(val, source, line);
            if (c.feeBps < 0) fail(source, line, "fee_bps must be >= 0");
        } else if (key == "slippage_bps") {
            c.slippageBps = number(val, source, line);
            if (c.slippageBps < 0) fail(source, line, "slippage_bps must be >= 0");
        } else if (key == "wf_train" || key == "wf_test" || key == "wf_step") {
            const double d = number(val, source, line);
            if (d < 1) fail(source, line, key + " must be >= 1");
            (key == "wf_train" ? c.wfTrain : key == "wf_test" ? c.wfTest : c.wfStep) = static_cast<size_t>(d);
        } else if (key == "min_trades_train") {
            c.minTradesTrain = static_cast<int>(number(val, source, line));
        } else if (key == "min_trades_oos") {
            c.minTradesOos = static_cast<int>(number(val, source, line));
        } else if (key == "poll_seconds") {
            c.pollSeconds = static_cast<int>(number(val, source, line));
            if (c.pollSeconds < 1) fail(source, line, "poll_seconds must be >= 1");
        } else if (key == "data_dir") {
            c.dataDir = val;
        } else if (key == "report_dir") {
            c.reportDir = val;
        } else if (key == "log_dir") {
            c.logDir = val;
        } else {
            fail(source, line, "unknown key '" + key + "'");
        }
    }
    return c;
}

Config Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return Config();  // optional file
    std::stringstream ss;
    ss << in.rdbuf();
    return fromText(ss.str(), path);
}

WalkForwardConfig Config::walkForward() const {
    WalkForwardConfig w;
    w.trainBars = wfTrain;
    w.testBars = wfTest;
    w.stepBars = wfStep;
    w.minTradesTrain = minTradesTrain;
    w.initialEquity = initialEquity;
    return w;
}

}  // namespace quant
