#pragma once

#include <cstddef>
#include <vector>

#include "quant/models/Model.hpp"

namespace quant {

// y = alpha + beta * x over the last W points, solved in closed form from running sums.
class RollingOLS : public Model {
public:
    explicit RollingOLS(size_t window);
    void reset() override;
    void update(double x, double y);
    void update(const std::vector<double>& x, double y) override { update(x.at(0), y); }
    double predict(const std::vector<double>& x) const override { return alpha_ + beta_ * x.at(0); }

    bool ready() const { return count_ >= cap_; }
    double alpha() const { return alpha_; }
    double beta() const { return beta_; }
    double residualStd() const { return resStd_; }

private:
    void refreshSums();
    size_t cap_;
    std::vector<double> xs_, ys_;
    size_t head_ = 0, count_ = 0, sinceRefresh_ = 0;
    double sx_ = 0, sy_ = 0, sxx_ = 0, sxy_ = 0, syy_ = 0;
    double alpha_ = 0, beta_ = 0, resStd_ = 0;
};

// Random-walk state [alpha, beta], observation y = alpha + beta * x + noise.
class KalmanFilter : public Model {
public:
    KalmanFilter(double delta = 1e-5, double observationNoise = 1e-3);
    void reset() override;
    void update(double x, double y);
    void update(const std::vector<double>& x, double y) override { update(x.at(0), y); }
    double predict(const std::vector<double>& x) const override { return alpha_ + beta_ * x.at(0); }

    double alpha() const { return alpha_; }
    double beta() const { return beta_; }
    double innovation() const { return e_; }          // y - prediction made BEFORE seeing y
    double innovationStd() const { return sqrtQ_; }   // sqrt of the innovation variance
    double zscore() const { return sqrtQ_ > 0 ? e_ / sqrtQ_ : 0.0; }
    size_t count() const { return count_; }

private:
    double delta_, ve_, vw_;
    double alpha_ = 0, beta_ = 0;
    double p_[2][2];
    double e_ = 0, sqrtQ_ = 0;
    size_t count_ = 0;
};

// Binary logistic regression on standardised inputs. Batch gradient descent in fit(), single SGD
// step in update(). Standardisation statistics come from fit() only (the training window).
class LogisticRegression : public Model {
public:
    LogisticRegression(size_t dim = 0, double lr = 0.05, double l2 = 0.0, int epochs = 50);
    void reset() override;  // forgets weights AND standardisation statistics
    void fit(const std::vector<std::vector<double>>& X, const std::vector<double>& y) override;
    void update(const std::vector<double>& x, double y) override;
    double predict(const std::vector<double>& x) const override;  // P(y = 1)

private:
    std::vector<double> standardise(const std::vector<double>& x) const;
    size_t dim_;
    double lr_, l2_;
    int epochs_;
    std::vector<double> mean_, std_, w_;
    double b_ = 0;
};

// Dickey-Fuller t-statistic for x_t: regress dx_t = a + g * x_{t-1} + e and return t(g).
// Report-only diagnostic. Compare with MacKinnon / Engle-Granger critical values:
// roughly -3.34 at 5% for a two-variable residual with a constant (approximate).
double dickeyFullerStat(const std::vector<double>& x);

}  // namespace quant
