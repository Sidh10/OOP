#include "quant/core/Logger.hpp"

#include <fstream>
#include <iostream>
#include <mutex>

#include "quant/core/TimeUtil.hpp"

namespace quant {

namespace {
std::mutex gMutex;
std::string gPath;
bool gConsole = true;

const char* tag(Logger::Level l) {
    switch (l) {
        case Logger::Level::Info: return "INFO";
        case Logger::Level::Warn: return "WARN";
        default: return "ERROR";
    }
}
}  // namespace

void Logger::log(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(gMutex);
    const std::string line = formatUtcFull(nowMs()) + " [" + tag(level) + "] " + msg;
    if (gConsole) std::cerr << line << "\n";
    if (!gPath.empty()) {
        std::ofstream f(gPath, std::ios::app);
        if (f) f << line << "\n";  // a missing logs/ folder must not crash the program
    }
}

void Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(gMutex);
    gPath = path;
}

void Logger::setConsoleEnabled(bool on) {
    std::lock_guard<std::mutex> lock(gMutex);
    gConsole = on;
}

}  // namespace quant
