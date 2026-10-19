#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include "quant/core/Errors.hpp"

namespace quant {

// Fixed-capacity ring buffer with O(1) push and running mean/variance.
// Welford-style add/remove updates drift slowly, so the statistics are recomputed
// exactly once every `capacity` pushes (amortised O(1)); this keeps 100k pushes accurate.
template <typename T>
class RollingWindow {
public:
    explicit RollingWindow(size_t capacity) : cap_(capacity), buf_(capacity) {
        if (capacity == 0) throw DataError("RollingWindow capacity must be > 0");
    }

    void push(T x) {
        const double v = static_cast<double>(x);
        if (count_ == cap_) {
            const double old = static_cast<double>(buf_[head_]);
            if (cap_ == 1) {
                mean_ = 0;
                m2_ = 0;
            } else {
                // Welford downdate: remove the oldest value (n -> n-1) ...
                const double delta = old - mean_;
                mean_ -= delta / static_cast<double>(cap_ - 1);
                m2_ -= delta * (old - mean_);
                if (m2_ < 0) m2_ = 0;
            }
            buf_[head_] = x;
            head_ = (head_ + 1) % cap_;
            // ... then add the new one (n-1 -> n).
            const double d2 = v - mean_;
            mean_ += d2 / static_cast<double>(cap_);
            m2_ += d2 * (v - mean_);
            if (++sinceRefresh_ >= cap_) refresh();
        } else {
            buf_[(head_ + count_) % cap_] = x;
            ++count_;
            const double d = v - mean_;
            mean_ += d / static_cast<double>(count_);
            m2_ += d * (v - mean_);
        }
    }

    size_t size() const { return count_; }
    size_t capacity() const { return cap_; }
    bool full() const { return count_ == cap_; }
    double mean() const { return count_ ? mean_ : 0.0; }
    // Sample variance (n-1); 0 with fewer than 2 points.
    double variance() const { return count_ > 1 ? m2_ / static_cast<double>(count_ - 1) : 0.0; }
    double stddev() const { return std::sqrt(variance()); }
    // i = 0 is the oldest element in the window.
    T at(size_t i) const { return buf_[(head_ + i) % cap_]; }
    T newest() const { return buf_[(head_ + count_ - 1) % cap_]; }

    void clear() {
        count_ = head_ = sinceRefresh_ = 0;
        mean_ = m2_ = 0;
    }

private:
    void refresh() {
        sinceRefresh_ = 0;
        double s = 0;
        for (size_t i = 0; i < count_; ++i) s += static_cast<double>(at(i));
        mean_ = s / static_cast<double>(count_);
        double acc = 0;
        for (size_t i = 0; i < count_; ++i) {
            const double d = static_cast<double>(at(i)) - mean_;
            acc += d * d;
        }
        m2_ = acc;
    }

    size_t cap_;
    std::vector<T> buf_;
    size_t head_ = 0;  // index of the oldest element
    size_t count_ = 0;
    size_t sinceRefresh_ = 0;
    double mean_ = 0;
    double m2_ = 0;
};

}  // namespace quant
