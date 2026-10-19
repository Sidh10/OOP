#pragma once

#include <stdexcept>
#include <string>

namespace quant {

// One root type so menu code can catch QuantError once and show a readable line.
class QuantError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// httpStatus is 0 for connection-level failures; retryAfterSec is -1 when absent.
class NetworkError : public QuantError {
public:
    explicit NetworkError(const std::string& msg, int httpStatus = 0, int retryAfterSec = -1)
        : QuantError(msg), status_(httpStatus), retryAfter_(retryAfterSec) {}
    int httpStatus() const { return status_; }
    int retryAfterSec() const { return retryAfter_; }

private:
    int status_;
    int retryAfter_;
};

class ParseError : public QuantError {
public:
    using QuantError::QuantError;
};

class DataError : public QuantError {
public:
    using QuantError::QuantError;
};

class ConfigError : public QuantError {
public:
    using QuantError::QuantError;
};

}  // namespace quant
