#pragma once

#include <string>

namespace quant {

// Small static logger: INFO/WARN/ERROR to stderr and, once enabled, to a log file.
// Never call it inside inner backtest loops (SPEC section 11).
class Logger {
public:
    enum class Level { Info, Warn, Error };

    static void info(const std::string& msg) { log(Level::Info, msg); }
    static void warn(const std::string& msg) { log(Level::Warn, msg); }
    static void error(const std::string& msg) { log(Level::Error, msg); }

    static void log(Level level, const std::string& msg);
    static void setLogFile(const std::string& path);  // empty string disables file logging
    static void setConsoleEnabled(bool on);           // tests silence the console
};

}  // namespace quant
