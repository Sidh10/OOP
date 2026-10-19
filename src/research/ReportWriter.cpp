#include "quant/research/ReportWriter.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"

namespace quant {

namespace {
const char* kHeader =
    "rank,universe,strategy,verdict,score,oos_sharpe,oos_sortino,oos_cagr,oos_maxdd,oos_trades,"
    "oos_winrate,is_sharpe,overfit_gap,baseline_sharpe,baseline_cagr,params_latest,n_windows,"
    "period_start,period_end";

std::string num(double v) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.6f", v);
    return buf;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cell;
    std::istringstream ss(line);
    while (std::getline(ss, cell, ',')) out.push_back(cell);
    return out;
}

double toD(const std::string& s, const std::string& path, size_t row) {
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size())
        throw ParseError("bad number '" + s + "' in " + path + " row " + std::to_string(row));
    return d;
}
}  // namespace

const char* ReportWriter::disclaimer() {
    return "Research output, not financial advice. Past performance does not predict future results.";
}

void ReportWriter::writeCsv(const std::string& path, const std::vector<Recommendation>& rows) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw DataError("cannot write report " + path);
    out << kHeader << "\n";
    for (const auto& r : rows) {
        out << (r.rank ? std::to_string(r.rank) : "-") << "," << r.universe << "," << r.strategy << ","
            << toString(r.verdict) << "," << num(r.score) << "," << num(r.oosSharpe) << ","
            << num(r.oosSortino) << "," << num(r.oosCagr) << "," << num(r.oosMaxDD) << "," << r.oosTrades
            << "," << num(r.oosWinRate) << "," << num(r.isSharpe) << "," << num(r.overfitGap) << ","
            << num(r.baselineSharpe) << "," << num(r.baselineCagr) << "," << r.paramsLatest << ","
            << r.windows << "," << r.periodStart << "," << r.periodEnd << "\n";
    }
    if (!out) throw DataError("write failed for report " + path);
}

std::vector<Recommendation> ReportWriter::readCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw DataError("cannot open report " + path + " (run option 4 first)");
    std::string line;
    if (!std::getline(in, line) || line.substr(0, 4) != "rank")
        throw ParseError("report " + path + " has no valid header");
    std::vector<Recommendation> rows;
    size_t row = 1;
    while (std::getline(in, line)) {
        ++row;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = split(line);
        // params_latest may be empty, which makes split() drop nothing in the middle but shortens the tail
        if (f.size() < 17) throw ParseError("report " + path + " row " + std::to_string(row) + ": too few columns");
        Recommendation r;
        r.rank = f[0] == "-" ? 0 : static_cast<size_t>(toD(f[0], path, row));
        r.universe = f[1];
        r.strategy = f[2];
        r.verdict = verdictFromString(f[3]);
        r.score = toD(f[4], path, row);
        r.oosSharpe = toD(f[5], path, row);
        r.oosSortino = toD(f[6], path, row);
        r.oosCagr = toD(f[7], path, row);
        r.oosMaxDD = toD(f[8], path, row);
        r.oosTrades = static_cast<int>(toD(f[9], path, row));
        r.oosWinRate = toD(f[10], path, row);
        r.isSharpe = toD(f[11], path, row);
        r.overfitGap = toD(f[12], path, row);
        r.baselineSharpe = toD(f[13], path, row);
        r.baselineCagr = toD(f[14], path, row);
        r.paramsLatest = f[15];
        r.windows = static_cast<size_t>(toD(f[16], path, row));
        if (f.size() > 17) r.periodStart = f[17];
        if (f.size() > 18) r.periodEnd = f[18];
        rows.push_back(r);
    }
    return rows;
}

void ReportWriter::printReport(std::ostream& os, const RecommendationSet& set) {
    os << "\n=== Strategy recommendation (walk-forward, out-of-sample) ===\n";
    os << std::left << std::setw(5) << "rank" << std::setw(16) << "universe" << std::setw(15) << "strategy"
       << std::setw(13) << "verdict" << std::right << std::setw(8) << "score" << std::setw(8) << "OOSsh"
       << std::setw(9) << "OOScagr" << std::setw(8) << "OOSdd" << std::setw(7) << "trades" << std::setw(8)
       << "gap" << std::setw(8) << "B&Hsh" << std::setw(9) << "B&Hcagr" << std::setw(5) << "win" << "\n";
    os << std::fixed << std::setprecision(2);
    for (const auto& r : set.rows) {
        os << std::left << std::setw(5) << (r.rank ? std::to_string(r.rank) : "-") << std::setw(16) << r.universe
           << std::setw(15) << r.strategy << std::setw(13) << toString(r.verdict) << std::right << std::setw(8)
           << r.score << std::setw(8) << r.oosSharpe << std::setw(8) << r.oosCagr * 100 << "%" << std::setw(7)
           << r.oosMaxDD * 100 << "%" << std::setw(7) << r.oosTrades << std::setw(8) << r.overfitGap
           << std::setw(8) << r.baselineSharpe << std::setw(8) << r.baselineCagr * 100 << "%" << std::setw(5)
           << r.windows << "\n";
    }
    os << "(OOSsh/OOScagr/OOSdd = stitched out-of-sample Sharpe/CAGR/max drawdown; gap = in-sample minus "
          "out-of-sample Sharpe; B&H = buy-and-hold on the same bars; last column = walk-forward windows used)\n";

    if (!set.pairDiagnostics.empty()) {
        os << "\nPair diagnostics (report-only, never used to gate trading):\n";
        for (const auto& d : set.pairDiagnostics)
            os << "  " << std::setw(18) << std::left << d.pair << std::right << " hedge beta=" << std::setprecision(3)
               << d.beta << "  Dickey-Fuller t=" << d.adfStat << "\n";
        os << "  Rough 5% critical value for an Engle-Granger residual with a constant is about -3.34 "
              "(MacKinnon, approximate); t above it means cointegration is not supported on this sample.\n";
    }

    os << "\n";
    if (!set.anyRecommended())
        os << "RESULT: no strategy beat the buy-and-hold baseline out of sample on this data.\n";
    else {
        os << "RECOMMENDED:\n";
        for (const auto& r : set.rows)
            if (r.verdict == Verdict::Recommended)
                os << "  #" << r.rank << " " << r.strategy << " on " << r.universe << " [" << r.paramsLatest
                   << "]\n";
    }
    os << "\nCaveats:\n"
          "  - With grids, strategies and universes tested together, some winners are luck (multiple testing).\n"
          "  - Shorts are simulated at the fill price with NO borrow or funding cost; real shorting costs more.\n"
          "  - Each walk-forward window starts flat, so entry costs are paid again: this is conservative.\n"
          "  - Crypto history is short and regime-dependent.\n";
    os << disclaimer() << "\n";
}

std::string ReportWriter::saveReports(const std::string& dir, const RecommendationSet& set, int64_t nowMs) {
    const std::string stamped = dir + "/recommendation_" + timestampForFilename(nowMs) + ".csv";
    writeCsv(stamped, set.rows);
    writeCsv(dir + "/latest_recommendation.csv", set.rows);
    return stamped;
}

}  // namespace quant
