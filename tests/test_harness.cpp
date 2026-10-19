#include <stdexcept>
#include "minitest.hpp"

TEST(harness_passes) {
    CHECK(1 + 1 == 2);
    CHECK_NEAR(0.1 + 0.2, 0.3, 1e-12);
    CHECK_THROWS(throw std::runtime_error("x"), std::runtime_error);
}
