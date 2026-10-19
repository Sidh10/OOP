#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>

#include "minitest.hpp"
#include "quant/Config.hpp"
#include "quant/core/FileUtil.hpp"
#include "quant/core/Logger.hpp"
#include "quant/research/ReportWriter.hpp"
#include "quant/ui/MenuApp.hpp"
#include "test_support.hpp"

using namespace quant;
using namespace testsupport;

namespace {
Config testConfig() {
    Config c;
    c.symbols = {"AAAUSDT"};
    c.pairs = {};
    c.dataDir = "logs/_menu_data";
    c.reportDir = "logs/_menu_reports";
    c.logDir = "logs";
    c.baseUrls = {"https://a.test"};
    c.historyDays = 10;
    return c;
}

struct Run {
    int code;
    std::string text;
};

Run runMenu(const std::string& input, std::shared_ptr<IHttpClient> http = std::make_shared<MockHttpClient>(),
            Config cfg = testConfig()) {
    Logger::setConsoleEnabled(false);
    std::istringstream in(input);
    std::ostringstream out;
    MenuApp app(cfg, std::move(http), in, out);
    int code = app.run();
    return {code, out.str()};
}
}  // namespace

TEST(menu_smoke_summary_then_exit_returns_zero) {  // SPEC test 16
    Run r = runMenu("2\n0\n");
    CHECK(r.code == 0);
    CHECK(r.text.find("Research output, not financial advice") != std::string::npos);  // startup disclaimer
    CHECK(r.text.find("Data summary") != std::string::npos);
    CHECK(r.text.find("1) Download / update data") != std::string::npos);
    CHECK(r.text.find("6) Show last recommendation report") != std::string::npos);
}

TEST(menu_end_of_input_exits_cleanly) {
    CHECK(runMenu("").code == 0);
    CHECK(runMenu("2\n").code == 0);  // EOF right after an option, no "0"
}

TEST(menu_survives_invalid_input) {
    Run r = runMenu("abc\n99\n\n-1\n3\nzzz\n0\n");
    CHECK(r.code == 0);
    CHECK(r.text.find("Unknown option 'abc'") != std::string::npos);
    CHECK(r.text.find("Not a valid choice") != std::string::npos);
}

TEST(menu_errors_are_one_readable_line_not_a_crash) {
    // option 4 with no cached data, option 6 with no report, option 3 on a symbol without data
    Run r = runMenu("4\n6\n3\n1\n1\n\n0\n");
    CHECK(r.code == 0);
    CHECK(r.text.find("Error: no cached data for AAAUSDT") != std::string::npos);
    CHECK(r.text.find("Error: cannot open report") != std::string::npos);
}

TEST(menu_download_summary_and_backtest_with_mock_http) {
    ensureDirectory("logs");
    ensureDirectory("logs/_menu_data");
    std::remove("logs/_menu_data/AAAUSDT_1h.csv");
    auto http = std::make_shared<MockHttpClient>();
    http->queueResponse(klinesJson(1000 * H, 50));  // one short page of synthetic candles
    // 1 = download, 2 = summary, 3 = backtest: strategy 1 (BuyAndHold), symbol 1, default params, then exit
    Run r = runMenu("1\n2\n3\n1\n1\n\n0\n", http);
    CHECK(r.code == 0);
    CHECK(r.text.find("1 of 1 symbols updated") != std::string::npos);
    CHECK(r.text.find("50 bars") != std::string::npos);
    CHECK(r.text.find("BuyAndHold") != std::string::npos);
    CHECK(r.text.find("in-sample backtest") != std::string::npos);
    CHECK(http->requests().size() == 1);
    CHECK(http->requests()[0].find("https://a.test/api/v3/klines?symbol=AAAUSDT&interval=1h") == 0);
    std::remove("logs/_menu_data/AAAUSDT_1h.csv");
}

TEST(menu_download_reports_failures_and_continues) {
    auto http = std::make_shared<MockHttpClient>();
    http->queueError(451);  // the only base URL is blocked
    Run r = runMenu("1\n0\n", http);
    CHECK(r.code == 0);
    CHECK(r.text.find("failed:") != std::string::npos);
    CHECK(r.text.find("0 of 1 symbols updated") != std::string::npos);
}

TEST(menu_option_6_shows_saved_report) {
    ensureDirectory("logs/_menu_reports");
    Recommendation r;
    r.rank = 1;
    r.universe = "AAAUSDT";
    r.strategy = "Momentum";
    r.verdict = Verdict::Reject;
    ReportWriter::writeCsv("logs/_menu_reports/latest_recommendation.csv", {r});
    Run run = runMenu("6\n0\n");
    CHECK(run.text.find("Momentum") != std::string::npos);
    CHECK(run.text.find("no strategy beat the buy-and-hold baseline") != std::string::npos);
    std::remove("logs/_menu_reports/latest_recommendation.csv");
}

TEST(live_without_report_or_recommendation_is_explained) {
    // no report file at all
    Run a = runMenu("5\n0\n");
    CHECK(a.text.find("run option 4 first") != std::string::npos);
    // report exists but nothing RECOMMENDED, user declines shadow mode
    ensureDirectory("logs/_menu_reports");
    Recommendation r;
    r.rank = 1;
    r.universe = "AAAUSDT";
    r.strategy = "Momentum";
    r.verdict = Verdict::Weak;
    ReportWriter::writeCsv("logs/_menu_reports/latest_recommendation.csv", {r});
    Run b = runMenu("5\nn\n0\n");
    CHECK(b.code == 0);
    CHECK(b.text.find("No strategy has a RECOMMENDED verdict") != std::string::npos);
    CHECK(b.text.find("NOT recommended") != std::string::npos);
    std::remove("logs/_menu_reports/latest_recommendation.csv");
}
