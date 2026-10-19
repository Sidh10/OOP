#pragma once

#include <cstdint>
#include <string>

namespace quant {

int64_t nowMs();                                    // wall clock, ms since epoch (UTC)
int64_t intervalToMs(const std::string& interval);  // "1h" -> 3600000; throws ConfigError if unknown
std::string formatUtc(int64_t ms);                  // "2026-10-06 18:00Z"
std::string formatUtcFull(int64_t ms);              // "2026-10-06 18:00:00Z"
std::string timestampForFilename(int64_t ms);       // "20261006_180000"

}  // namespace quant
