#include "quant/research/StrategyRecommender.hpp"

#include <algorithm>
#include <cmath>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"
#include "quant/models/Models.hpp"
#include "quant/strategies/StrategyFactory.hpp"

namespace quant {

const char* toString(Verdict v) {
    switch (v) {
        case Verdict::Recommended: return "RECOMMENDED";
        case Verdict::Weak: return "WEAK";
        case Verdict::Reject: return "REJECT";
        default: return "INSUFFICIENT";
    }
}

Verdict verdictFromString(const std::string& s) {
    if (s == "RECOMMENDED") return Verdict::Recommended;
    if (s == "WEAK") return Verdict::Weak;
    if (s == "REJECT") return Verdict::Reject;
    if (s == "INSUFFICIENT") return Verdict::Insufficient;
    throw ParseError("unknown verdict '" + s + "'");
}

bool RecommendationSet::anyRecommended() const {
    for (const auto& r : rows)
        if (r.verdict == Verdict::Recommended) return true;
    return false;
}

StrategyRecommender::StrategyRecommender(WalkForwardConfig cfg, std::shared_ptr<const CostModel> cost,
                                         int minOosTrades)
    : cfg_(cfg), cost_(std::move(cost)), minOosTrades_(minOosTrades) {}

Verdict StrategyRecommender::decide(int oosTrades, double oosSharpe, double baselineSharpe,
                                    double overfitGap, int minOosTrades) {
    if (oosTrades < minOosTrades) return Verdict::Insufficient;
    if (oosSharpe > baselineSharpe + defaults::recommendSharpeMargin &&
        oosSharpe > defaults::recommendMinSharpe && overfitGap <= defaults::recommendMaxOverfitGap)
        return Verdict::Recommended;
    if (oosSharpe > 0) return Verdict::Weak;
    return Verdict::Reject;
}

double StrategyRecommender::score(double oosSharpe, double oosMaxDD, double overfitGap) {
    return oosSharpe - 1.0 * oosMaxDD - 0.5 * std::max(0.0, overfitGap);
}

Recommendation StrategyRecommender::toRecommendation(const WalkForwardReport& rep, int minOosTrades) {
    Recommendation r;
    r.universe = rep.universe;
    r.strategy = rep.strategy;
    r.windows = rep.usedWindows;
    if (rep.usedWindows == 0) {  // nothing qualified in any window
        r.verdict = Verdict::Insufficient;
        return r;
    }
    r.oosSharpe = rep.oos.sharpe();
    r.oosSortino = rep.oos.sortino();
    r.oosCagr = rep.oos.cagr();
    r.oosMaxDD = rep.oos.maxDrawdown();
    r.oosTrades = rep.oos.numTrades();
    r.oosWinRate = rep.oos.winRate();
    r.isSharpe = rep.isSharpe;
    r.overfitGap = rep.overfitGap;
    r.baselineSharpe = rep.baseline.sharpe();
    r.baselineCagr = rep.baseline.cagr();
    r.paramsLatest = rep.latestParams.toString();
    r.periodStart = formatUtc(rep.periodStart);
    r.periodEnd = formatUtc(rep.periodEnd);
    r.score = score(r.oosSharpe, r.oosMaxDD, r.overfitGap);
    r.verdict = decide(r.oosTrades, r.oosSharpe, r.baselineSharpe, r.overfitGap, minOosTrades);
    return r;
}

RecommendationSet StrategyRecommender::rank(
    const std::vector<CandleSeries>& series,
    const std::vector<std::pair<std::string, std::string>>& pairs, const Progress& progress) const {
    WalkForwardOptimizer wf(cfg_, cost_);
    RecommendationSet out;
    auto find = [&](const std::string& sym) -> const CandleSeries& {
        for (const auto& s : series)
            if (s.symbol() == sym) return s;
        throw DataError("no data loaded for " + sym);
    };

    for (const std::string& name : StrategyFactory::candidates()) {
        if (!StrategyFactory::isPairStrategy(name)) {
            for (const auto& s : series) {
                if (progress) progress("walk-forward " + name + " on " + s.symbol());
                out.rows.push_back(toRecommendation(wf.run(name, {s}), minOosTrades_));
            }
        } else {
            for (const auto& pr : pairs) {
                std::vector<CandleSeries> two{find(pr.first), find(pr.second)};
                alignSeries(two);
                if (progress) progress("walk-forward " + name + " on " + pr.first + "/" + pr.second);
                out.rows.push_back(toRecommendation(wf.run(name, two), minOosTrades_));
            }
        }
    }

    // Report-only pair diagnostics: full-sample OLS hedge ratio and Dickey-Fuller statistic.
    for (const auto& pr : pairs) {
        std::vector<CandleSeries> two{find(pr.first), find(pr.second)};
        alignSeries(two);
        if (two[0].size() < 50) continue;
        RollingOLS ols(two[0].size());
        for (size_t t = 0; t < two[0].size(); ++t)
            ols.update(std::log(two[1][t].close), std::log(two[0][t].close));
        std::vector<double> resid;
        for (size_t t = 0; t < two[0].size(); ++t)
            resid.push_back(std::log(two[0][t].close) - (ols.alpha() + ols.beta() * std::log(two[1][t].close)));
        out.pairDiagnostics.push_back({pr.first + "/" + pr.second, ols.beta(), dickeyFullerStat(resid)});
    }

    // Rank by score among everything that has enough trades; INSUFFICIENT rows go last, unranked.
    std::stable_sort(out.rows.begin(), out.rows.end(), [](const Recommendation& a, const Recommendation& b) {
        const bool ai = a.verdict == Verdict::Insufficient, bi = b.verdict == Verdict::Insufficient;
        if (ai != bi) return bi;  // non-insufficient first
        if (ai) return false;
        return a.score > b.score;
    });
    size_t rank = 1;
    for (auto& r : out.rows)
        if (r.verdict != Verdict::Insufficient) r.rank = rank++;
    return out;
}

}  // namespace quant
