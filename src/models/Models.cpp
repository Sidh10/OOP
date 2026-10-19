#include "quant/models/Models.hpp"

#include <algorithm>
#include <cmath>

#include "quant/core/Errors.hpp"

namespace quant {

void Model::fit(const std::vector<std::vector<double>>& X, const std::vector<double>& y) {
    if (X.size() != y.size()) throw DataError("Model::fit: X and y differ in length");
    reset();
    for (size_t i = 0; i < X.size(); ++i) update(X[i], y[i]);
}

// ---------------- RollingOLS ----------------
RollingOLS::RollingOLS(size_t window) : cap_(window), xs_(window), ys_(window) {
    if (window < 3) throw ConfigError("RollingOLS window must be >= 3");
}

void RollingOLS::reset() {
    head_ = count_ = sinceRefresh_ = 0;
    sx_ = sy_ = sxx_ = sxy_ = syy_ = 0;
    alpha_ = beta_ = resStd_ = 0;
}

void RollingOLS::refreshSums() {
    sx_ = sy_ = sxx_ = sxy_ = syy_ = 0;
    for (size_t i = 0; i < count_; ++i) {
        const double x = xs_[(head_ + i) % cap_], y = ys_[(head_ + i) % cap_];
        sx_ += x;
        sy_ += y;
        sxx_ += x * x;
        sxy_ += x * y;
        syy_ += y * y;
    }
    sinceRefresh_ = 0;
}

void RollingOLS::update(double x, double y) {
    if (count_ == cap_) {  // drop the oldest point
        const double ox = xs_[head_], oy = ys_[head_];
        sx_ -= ox;
        sy_ -= oy;
        sxx_ -= ox * ox;
        sxy_ -= ox * oy;
        syy_ -= oy * oy;
        xs_[head_] = x;
        ys_[head_] = y;
        head_ = (head_ + 1) % cap_;
    } else {
        xs_[(head_ + count_) % cap_] = x;
        ys_[(head_ + count_) % cap_] = y;
        ++count_;
    }
    sx_ += x;
    sy_ += y;
    sxx_ += x * x;
    sxy_ += x * y;
    syy_ += y * y;
    if (++sinceRefresh_ >= cap_) refreshSums();  // exact recompute stops add/subtract drift

    if (count_ < 3) return;
    const double n = static_cast<double>(count_);
    const double mx = sx_ / n, my = sy_ / n;
    const double Sxx = sxx_ - n * mx * mx;
    const double Sxy = sxy_ - n * mx * my;
    const double Syy = syy_ - n * my * my;
    if (Sxx / (n - 1.0) < 1e-12) return;  // regressor is constant: keep the previous estimate
    beta_ = Sxy / Sxx;
    alpha_ = my - beta_ * mx;
    resStd_ = std::sqrt(std::max(Syy - beta_ * Sxy, 0.0) / (n - 2.0));
}

// ---------------- KalmanFilter ----------------
KalmanFilter::KalmanFilter(double delta, double observationNoise)
    : delta_(delta), ve_(observationNoise), vw_(delta / (1.0 - delta)) {
    reset();
}

void KalmanFilter::reset() {
    alpha_ = beta_ = 0;
    // Start fairly uncertain so the first few observations pull the state in quickly.
    p_[0][0] = p_[1][1] = 1.0;
    p_[0][1] = p_[1][0] = 0.0;
    e_ = sqrtQ_ = 0;
    count_ = 0;
}

void KalmanFilter::update(double x, double y) {
    // Regressor vector h = [1, x] for state [alpha, beta].
    // Predict: state is a random walk, so only the covariance grows.
    const double r00 = p_[0][0] + vw_, r01 = p_[0][1], r10 = p_[1][0], r11 = p_[1][1] + vw_;
    const double yhat = alpha_ + beta_ * x;
    e_ = y - yhat;
    // Q = h R h' + Ve
    const double Rh0 = r00 + r01 * x;  // (R h')[0]
    const double Rh1 = r10 + r11 * x;  // (R h')[1]
    const double Q = Rh0 + x * Rh1 + ve_;
    sqrtQ_ = std::sqrt(Q);
    const double k0 = Rh0 / Q, k1 = Rh1 / Q;
    alpha_ += k0 * e_;
    beta_ += k1 * e_;
    // P = R - K h R
    const double hR0 = r00 + x * r10;  // (h R)[0]
    const double hR1 = r01 + x * r11;  // (h R)[1]
    p_[0][0] = r00 - k0 * hR0;
    p_[0][1] = r01 - k0 * hR1;
    p_[1][0] = r10 - k1 * hR0;
    p_[1][1] = r11 - k1 * hR1;
    ++count_;
}

// ---------------- LogisticRegression ----------------
LogisticRegression::LogisticRegression(size_t dim, double lr, double l2, int epochs)
    : dim_(dim), lr_(lr), l2_(l2), epochs_(epochs) {
    reset();
}

void LogisticRegression::reset() {
    mean_.assign(dim_, 0.0);
    std_.assign(dim_, 1.0);
    w_.assign(dim_, 0.0);
    b_ = 0;
}

std::vector<double> LogisticRegression::standardise(const std::vector<double>& x) const {
    std::vector<double> z(dim_);
    for (size_t j = 0; j < dim_; ++j) z[j] = (x[j] - mean_[j]) / std_[j];
    return z;
}

static double sigmoid(double t) {
    if (t >= 0) return 1.0 / (1.0 + std::exp(-t));
    const double e = std::exp(t);
    return e / (1.0 + e);
}

void LogisticRegression::fit(const std::vector<std::vector<double>>& X, const std::vector<double>& y) {
    if (X.size() != y.size()) throw DataError("LogisticRegression::fit: X and y differ in length");
    if (X.empty()) throw DataError("LogisticRegression::fit: no training data");
    if (dim_ == 0) dim_ = X[0].size();
    reset();
    const size_t n = X.size();
    for (size_t j = 0; j < dim_; ++j) {
        double m = 0;
        for (const auto& row : X) m += row[j];
        m /= static_cast<double>(n);
        double v = 0;
        for (const auto& row : X) v += (row[j] - m) * (row[j] - m);
        v = n > 1 ? v / static_cast<double>(n - 1) : 0.0;
        mean_[j] = m;
        std_[j] = std::sqrt(v) < 1e-12 ? 1.0 : std::sqrt(v);
    }
    std::vector<std::vector<double>> Z;
    Z.reserve(n);
    for (const auto& row : X) Z.push_back(standardise(row));

    std::vector<double> gw(dim_);
    for (int ep = 0; ep < epochs_; ++ep) {
        std::fill(gw.begin(), gw.end(), 0.0);
        double gb = 0;
        for (size_t i = 0; i < n; ++i) {
            double t = b_;
            for (size_t j = 0; j < dim_; ++j) t += w_[j] * Z[i][j];
            const double err = sigmoid(t) - y[i];
            for (size_t j = 0; j < dim_; ++j) gw[j] += err * Z[i][j];
            gb += err;
        }
        const double inv = 1.0 / static_cast<double>(n);
        for (size_t j = 0; j < dim_; ++j) w_[j] -= lr_ * (gw[j] * inv + l2_ * w_[j]);
        b_ -= lr_ * gb * inv;
    }
}

void LogisticRegression::update(const std::vector<double>& x, double y) {
    const std::vector<double> z = standardise(x);
    double t = b_;
    for (size_t j = 0; j < dim_; ++j) t += w_[j] * z[j];
    const double err = sigmoid(t) - y;
    for (size_t j = 0; j < dim_; ++j) w_[j] -= lr_ * (err * z[j] + l2_ * w_[j]);
    b_ -= lr_ * err;
}

double LogisticRegression::predict(const std::vector<double>& x) const {
    const std::vector<double> z = standardise(x);
    double t = b_;
    for (size_t j = 0; j < dim_; ++j) t += w_[j] * z[j];
    return sigmoid(t);
}

// ---------------- Dickey-Fuller ----------------
double dickeyFullerStat(const std::vector<double>& x) {
    if (x.size() < 10) throw DataError("dickeyFullerStat needs at least 10 points");
    // OLS of dx_t on [1, x_{t-1}]; t-statistic of the slope.
    const size_t n = x.size() - 1;
    double sx = 0, sy = 0;
    for (size_t t = 1; t <= n; ++t) {
        sx += x[t - 1];
        sy += x[t] - x[t - 1];
    }
    const double mx = sx / static_cast<double>(n), my = sy / static_cast<double>(n);
    double Sxx = 0, Sxy = 0, Syy = 0;
    for (size_t t = 1; t <= n; ++t) {
        const double a = x[t - 1] - mx, b = (x[t] - x[t - 1]) - my;
        Sxx += a * a;
        Sxy += a * b;
        Syy += b * b;
    }
    if (Sxx < 1e-18) return 0.0;
    const double g = Sxy / Sxx;
    const double sse = std::max(Syy - g * Sxy, 0.0);
    const double se = std::sqrt(sse / (static_cast<double>(n) - 2.0) / Sxx);
    return se < 1e-18 ? 0.0 : g / se;
}

}  // namespace quant
