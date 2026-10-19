#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "quant/research/WalkForwardOptimizer.hpp"

namespace quant {

enum class Verdict { Recommended, Weak, Reject, Insufficient };

const char* toString(Verdict v);
Verdict verdictFromString(const std::string& s);  // throws ParseError

// One row of the report: a (universe, strategy) pair evaluated out of sample.
struct Recommendation {
    size_t rank = 0;  // 0 = not ranked (INSUFFICIENT)
    std::string universe;
    std::string strategy;
    Verdict verdict = Verdict::Reject;
    double score = 0;
    double oosSharpe = 0, oosSortino = 0, oosCagr = 0, oosMaxDD = 0;
    int oosTrades = 0;
    double oosWinRate = 0, isSharpe = 0, overfitGap = 0;
    double baselineSharpe = 0, baselineCagr = 0;
    std::string paramsLatest;  // "k=v;k=v"
    size_t windows = 0;
    std::string periodStart, periodEnd;
};

// Report-only statistic for pairs (never used to gate trading, D-010).
struct PairDiagnostic {
    std::string pair;
    double beta = 0;     // full-sample OLS hedge ratio
    double adfStat = 0;  // Dickey-Fuller t-statistic of the spread residual
};

struct RecommendationSet {
    std::vector<Recommendation> rows;  // ranked rows first (best score first), then unranked
    std::vector<PairDiagnostic> pairDiagnostics;
    bool anyRecommended() const;
};

class StrategyRecommender {
public:
    using Progress = std::function<void(const std::string&)>;

    StrategyRecommender(WalkForwardConfig cfg, std::shared_ptr<const CostModel> cost,
                        int minOosTrades = defaults::minTradesOos);

    // Evaluates every candidate strategy on every universe: each single series for single-asset
    // strategies, each (A, B) pair for pairs strategies. Thresholds are fixed in Defaults.hpp.
    RecommendationSet rank(const std::vector<CandleSeries>& series,
                           const std::vector<std::pair<std::string, std::string>>& pairs,
                           const Progress& progress = nullptr) const;

    // Pure decision rule, public for testing (SPEC section 8).
    static Verdict decide(int oosTrades, double oosSharpe, double baselineSharpe, double overfitGap,
                          int minOosTrades = defaults::minTradesOos);
    static double score(double oosSharpe, double oosMaxDD, double overfitGap);

    static Recommendation toRecommendation(const WalkForwardReport& rep, int minOosTrades);

private:
    WalkForwardConfig cfg_;
    std::shared_ptr<const CostModel> cost_;
    int minOosTrades_;
};

}  // namespace quant
