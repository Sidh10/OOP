// Tiny test harness: auto-registering TEST cases, a few CHECK macros.
// Failures are counted and reported; the runner in test_main.cpp returns non-zero.
#pragma once

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace minitest {

struct Case {
    std::string name;
    void (*fn)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;  // function-local static: safe init order across files
    return cases;
}

inline int& failureCount() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void fail(const char* file, int line, const std::string& msg) {
    ++failureCount();
    std::cerr << "  FAIL " << file << ":" << line << "  " << msg << "\n";
}

}  // namespace minitest

#define TEST(name)                                                \
    static void name();                                           \
    static minitest::Registrar minitest_reg_##name(#name, name);  \
    static void name()

#define CHECK(expr)                                                      \
    do {                                                                 \
        if (!(expr)) minitest::fail(__FILE__, __LINE__, "CHECK(" #expr ")"); \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                     \
    do {                                                                          \
        double minitest_a = static_cast<double>(a);                               \
        double minitest_b = static_cast<double>(b);                               \
        if (!(std::fabs(minitest_a - minitest_b) <= (tol)))                       \
            minitest::fail(__FILE__, __LINE__,                                    \
                           "CHECK_NEAR(" #a ", " #b ") got " + std::to_string(minitest_a) + \
                               " vs " + std::to_string(minitest_b));              \
    } while (0)

// Passes only if `expr` throws exactly something catchable as ExType.
#define CHECK_THROWS(expr, ExType)                                                  \
    do {                                                                            \
        bool minitest_threw = false;                                                \
        try {                                                                       \
            (void)(expr);                                                           \
        } catch (const ExType&) {                                                   \
            minitest_threw = true;                                                  \
        } catch (...) {                                                             \
        }                                                                           \
        if (!minitest_threw)                                                        \
            minitest::fail(__FILE__, __LINE__, "CHECK_THROWS(" #expr ", " #ExType ")"); \
    } while (0)
