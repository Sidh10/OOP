#include "quant/data/IHttpClient.hpp"

#include "quant/core/Errors.hpp"

namespace quant {

void MockHttpClient::queueResponse(std::string body) { queue_.push_back({std::move(body), 0, -1}); }

void MockHttpClient::queueError(int httpStatus, int retryAfterSec) {
    queue_.push_back({"", httpStatus, retryAfterSec});
}

std::string MockHttpClient::get(const std::string& url) {
    requests_.push_back(url);
    if (queue_.empty()) throw NetworkError("MockHttpClient: no response queued for " + url);
    Item it = std::move(queue_.front());
    queue_.pop_front();
    if (it.status != 0)
        throw NetworkError("mock HTTP " + std::to_string(it.status), it.status, it.retryAfter);
    return it.body;
}

}  // namespace quant
