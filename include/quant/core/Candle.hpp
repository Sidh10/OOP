#pragma once

#include <cstdint>
#include <iosfwd>

namespace quant {

struct Candle {
    int64_t openTime = 0;  // ms since epoch, UTC
    int64_t closeTime = 0;
    double open = 0, high = 0, low = 0, close = 0, volume = 0;
};

std::ostream& operator<<(std::ostream& os, const Candle& c);

}  // namespace quant
