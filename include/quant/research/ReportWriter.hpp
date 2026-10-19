#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "quant/research/StrategyRecommender.hpp"

namespace quant {

class ReportWriter {
public:
    static const char* disclaimer();  // "Research output, not financial advice. ..."

    // CSV with the exact column list of SPEC section 8.
    static void writeCsv(const std::string& path, const std::vector<Recommendation>& rows);
    static std::vector<Recommendation> readCsv(const std::string& path);  // throws DataError/ParseError

    // Console table plus caveats, pair diagnostics and the honest "nothing beat buy-and-hold" line.
    static void printReport(std::ostream& os, const RecommendationSet& set);

    // Writes reports/recommendation_<timestamp>.csv and an identical reports/latest_recommendation.csv.
    // Returns the timestamped path.
    static std::string saveReports(const std::string& dir, const RecommendationSet& set, int64_t nowMs);
};

}  // namespace quant
