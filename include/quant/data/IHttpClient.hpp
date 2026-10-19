#pragma once

#include <deque>
#include <string>
#include <vector>

namespace quant {

// Narrow interface so the data source can be tested without a network (dependency injection).
// get() returns the response body; on HTTP status >= 400 or a transport failure it throws NetworkError.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual std::string get(const std::string& url) = 0;
};

// Test double: replays queued responses (or errors) in order and records the requested URLs.
class MockHttpClient : public IHttpClient {
public:
    void queueResponse(std::string body);
    void queueError(int httpStatus, int retryAfterSec = -1);
    std::string get(const std::string& url) override;
    const std::vector<std::string>& requests() const { return requests_; }

private:
    struct Item {
        std::string body;
        int status = 0;  // 0 = success
        int retryAfter = -1;
    };
    std::deque<Item> queue_;
    std::vector<std::string> requests_;
};

}  // namespace quant
