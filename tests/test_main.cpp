#include <cstring>
#include <exception>

#include "minitest.hpp"

// Usage: quant_tests [substring]   (runs only tests whose name contains the substring)
int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;
    for (const auto& c : minitest::registry()) {
        if (filter && c.name.find(filter) == std::string::npos) continue;
        int before = minitest::failureCount();
        std::cerr << "[ RUN  ] " << c.name << "\n";
        try {
            c.fn();
        } catch (const std::exception& e) {
            minitest::fail(__FILE__, __LINE__, std::string("uncaught exception: ") + e.what());
        } catch (...) {
            minitest::fail(__FILE__, __LINE__, "uncaught non-std exception");
        }
        ++run;
        if (minitest::failureCount() != before) {
            ++failedTests;
            std::cerr << "[ FAIL ] " << c.name << "\n";
        }
    }
    std::cout << run << " tests run, " << failedTests << " failed\n";
    return failedTests == 0 ? 0 : 1;
}
