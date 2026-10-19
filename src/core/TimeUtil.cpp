#include "quant/core/TimeUtil.hpp"

#include <chrono>
#include <cstdio>
#include <map>

#include "quant/core/Errors.hpp"

namespace quant {

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int64_t intervalToMs(const std::string& interval) {
    // 1M is calendar-based on Binance; 30 days is only an approximation.
    static const std::map<std::string, int64_t> table = {
        {"1s", 1000LL},      {"1m", 60000LL},     {"3m", 180000LL},    {"5m", 300000LL},
        {"15m", 900000LL},   {"30m", 1800000LL},  {"1h", 3600000LL},   {"2h", 7200000LL},
        {"4h", 14400000LL},  {"6h", 21600000LL},  {"8h", 28800000LL},  {"12h", 43200000LL},
        {"1d", 86400000LL},  {"3d", 259200000LL}, {"1w", 604800000LL}, {"1M", 2592000000LL}};
    auto it = table.find(interval);
    if (it == table.end()) throw ConfigError("unknown interval '" + interval + "'");
    return it->second;
}

namespace {
struct Civil {
    int y, m, d, hh, mm, ss;
};

// Civil-from-days conversion; avoids gmtime portability and thread-safety issues.
Civil toCivil(int64_t ms) {
    int64_t secs = ms / 1000;
    if (ms < 0 && ms % 1000 != 0) --secs;
    int64_t days = secs / 86400;
    int64_t rem = secs % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    int64_t doe = days - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int64_t d = doy - (153 * mp + 2) / 5 + 1;
    int64_t m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) ++y;
    return {static_cast<int>(y),         static_cast<int>(m),
            static_cast<int>(d),         static_cast<int>(rem / 3600),
            static_cast<int>(rem % 3600 / 60), static_cast<int>(rem % 60)};
}
}  // namespace

std::string formatUtc(int64_t ms) {
    Civil c = toCivil(ms);
    char buf[48];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02dZ", c.y, c.m, c.d, c.hh, c.mm);
    return buf;
}

std::string formatUtcFull(int64_t ms) {
    Civil c = toCivil(ms);
    char buf[48];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02dZ", c.y, c.m, c.d, c.hh, c.mm,
                  c.ss);
    return buf;
}

std::string timestampForFilename(int64_t ms) {
    Civil c = toCivil(ms);
    char buf[48];
    std::snprintf(buf, sizeof buf, "%04d%02d%02d_%02d%02d%02d", c.y, c.m, c.d, c.hh, c.mm, c.ss);
    return buf;
}

}  // namespace quant
