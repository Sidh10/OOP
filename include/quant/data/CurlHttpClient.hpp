#pragma once

#include <string>

#include "quant/data/IHttpClient.hpp"

namespace quant {

// RAII wrapper around one libcurl easy handle: created in the constructor, always
// released in the destructor, even when get() throws. Not copyable.
class CurlHttpClient : public IHttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;
    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    std::string get(const std::string& url) override;

private:
    void* handle_;  // CURL*, kept opaque so only the .cpp includes <curl/curl.h>
    std::string caBundle_;
};

}  // namespace quant
