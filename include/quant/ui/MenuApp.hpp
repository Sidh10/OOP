#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "quant/Config.hpp"
#include "quant/data/IHttpClient.hpp"

namespace quant {

// Numbered CLI menu (SPEC section 10). Streams and the HTTP client are injected, so tests can
// drive it with a string stream and a mock client. Exceptions never escape a menu option.
class MenuApp {
public:
    MenuApp(Config cfg, std::shared_ptr<IHttpClient> http, std::istream& in, std::ostream& out);

    int run();  // interactive loop; returns 0, also on end of input
    // Option 5 without the menu (--live). `shadow` allows monitoring a strategy that is not RECOMMENDED.
    int runLive(bool once, bool shadow);

private:
    bool readLine(std::string& line);
    // Shows a numbered list and reads a choice by number or name; returns -1 on bad input/EOF.
    int pick(const std::string& title, const std::vector<std::string>& options);

    void optionDownload();
    void optionSummary();
    void optionBacktest();
    void optionRecommend();
    void optionLive();
    void optionReport();

    std::vector<CandleSeries> loadSeries(const std::vector<std::string>& symbols) const;
    void printBanner();

    Config cfg_;
    std::shared_ptr<IHttpClient> http_;
    std::istream& in_;
    std::ostream& out_;
};

// Entry point shared by src/main.cpp and the dev driver: parses flags, builds the client-backed
// app and runs it. Flags: --config FILE, --live, --once, --shadow, --help.
int runCli(int argc, char** argv, std::shared_ptr<IHttpClient> http);

}  // namespace quant
