#include "quant/data/CurlHttpClient.hpp"

#include <curl/curl.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <mutex>

#include "quant/core/Errors.hpp"

namespace quant {

namespace {

size_t writeBody(char* data, size_t size, size_t n, void* user) {
    static_cast<std::string*>(user)->append(data, size * n);
    return size * n;
}

// Captures only the Retry-After header; other headers are ignored.
size_t writeHeader(char* data, size_t size, size_t n, void* user) {
    const size_t len = size * n;
    std::string line(data, len);
    const std::string key = "retry-after:";
    if (line.size() > key.size()) {
        std::string head = line.substr(0, key.size());
        for (auto& ch : head) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (head == key) *static_cast<int*>(user) = std::atoi(line.c_str() + key.size());
    }
    return len;
}

void globalInitOnce() {
    static std::once_flag flag;
    std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

// MSYS2 builds of libcurl often cannot find a CA bundle outside the MSYS2 shell.
// We look for one; verification itself is never disabled.
std::string findCaBundle() {
    const char* envs[] = {"CURL_CA_BUNDLE", "SSL_CERT_FILE"};
    for (const char* e : envs) {
        const char* v = std::getenv(e);
        if (v && std::ifstream(v).good()) return v;
    }
    const char* known[] = {"C:/msys64/ucrt64/etc/ssl/certs/ca-bundle.crt",
                           "C:/msys64/mingw64/etc/ssl/certs/ca-bundle.crt",
                           "C:/msys64/usr/ssl/certs/ca-bundle.crt",
                           "C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt"};
    for (const char* k : known)
        if (std::ifstream(k).good()) return k;
    return "";
}

}  // namespace

CurlHttpClient::CurlHttpClient() : handle_(nullptr) {
    globalInitOnce();
    handle_ = curl_easy_init();
    if (!handle_) throw NetworkError("curl_easy_init failed");
    caBundle_ = findCaBundle();
}

CurlHttpClient::~CurlHttpClient() {
    if (handle_) curl_easy_cleanup(static_cast<CURL*>(handle_));
}

std::string CurlHttpClient::get(const std::string& url) {
    CURL* h = static_cast<CURL*>(handle_);
    curl_easy_reset(h);
    std::string body;
    int retryAfter = -1;
    curl_easy_setopt(h, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h, CURLOPT_HTTPGET, 1L);  // GET only, always
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(h, CURLOPT_HEADERDATA, &retryAfter);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "quant-engine-course-project/1.0");
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");  // let curl negotiate compression
    // Certificate verification stays ON (CURLOPT_SSL_VERIFYPEER default = 1).
    if (!caBundle_.empty()) {
        curl_easy_setopt(h, CURLOPT_CAINFO, caBundle_.c_str());
    } else {
#ifdef CURLSSLOPT_NATIVE_CA
        curl_easy_setopt(h, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif
    }

    CURLcode rc = curl_easy_perform(h);
    if (rc != CURLE_OK)
        throw NetworkError(std::string("curl error: ") + curl_easy_strerror(rc) + " (" + url + ")");
    long status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    if (status >= 400)
        throw NetworkError("HTTP " + std::to_string(status) + " from " + url + ": " + body.substr(0, 200),
                           static_cast<int>(status), retryAfter);
    return body;
}

}  // namespace quant
