#include "quant/ui/MenuApp.hpp"

#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "quant/core/Errors.hpp"
#include "quant/core/FileUtil.hpp"
#include "quant/core/Logger.hpp"
#include "quant/core/TimeUtil.hpp"
#include "quant/data/BinanceDataSource.hpp"
#include "quant/data/DataCache.hpp"
#include "quant/engine/Backtester.hpp"
#include "quant/live/LiveRunner.hpp"
#include "quant/research/ReportWriter.hpp"
#include "quant/strategies/StrategyFactory.hpp"

namespace quant {

namespace {
std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

extern "C" void onSigint(int) { LiveRunner::requestStop(); }  // only touches an atomic flag

std::string universeName(const std::pair<std::string, std::string>& p) { return p.first + "/" + p.second; }
}  // namespace

MenuApp::MenuApp(Config cfg, std::shared_ptr<IHttpClient> http, std::istream& in, std::ostream& out)
    : cfg_(std::move(cfg)), http_(std::move(http)), in_(in), out_(out) {}

bool MenuApp::readLine(std::string& line) {
    if (!std::getline(in_, line)) return false;
    line = trim(line);
    return true;
}

int MenuApp::pick(const std::string& title, const std::vector<std::string>& options) {
    out_ << title << "\n";
    for (size_t i = 0; i < options.size(); ++i) out_ << "  " << (i + 1) << ") " << options[i] << "\n";
    out_ << "> " << std::flush;
    std::string line;
    if (!readLine(line)) return -1;
    for (size_t i = 0; i < options.size(); ++i)
        if (line == options[i] || line == std::to_string(i + 1)) return static_cast<int>(i);
    out_ << "Not a valid choice: '" << line << "'\n";
    return -1;
}

void MenuApp::printBanner() {
    out_ << "AI Quant Trading & Statistical Arbitrage Engine\n"
         << "Backtests and signal display only. No orders are ever placed and no API keys are used.\n"
         << ReportWriter::disclaimer() << "\n";
}

std::vector<CandleSeries> MenuApp::loadSeries(const std::vector<std::string>& symbols) const {
    DataCache cache(cfg_.dataDir, nullptr);
    std::vector<CandleSeries> out;
    for (const auto& s : symbols) out.push_back(cache.load(s, cfg_.interval));
    return out;
}

int MenuApp::run() {
    printBanner();
    std::string line;
    while (true) {
        out_ << "\n1) Download / update data\n"
                "2) Show data summary\n"
                "3) Backtest one strategy\n"
                "4) Run walk-forward + recommend strategies\n"
                "5) Live signals\n"
                "6) Show last recommendation report\n"
                "0) Exit\n> "
             << std::flush;
        if (!readLine(line)) {
            out_ << "\n(end of input)\n";
            return 0;
        }
        if (line.empty()) continue;
        try {
            if (line == "0") return 0;
            else if (line == "1") optionDownload();
            else if (line == "2") optionSummary();
            else if (line == "3") optionBacktest();
            else if (line == "4") optionRecommend();
            else if (line == "5") optionLive();
            else if (line == "6") optionReport();
            else out_ << "Unknown option '" << line << "'. Please enter 0-6.\n";
        } catch (const std::exception& e) {  // QuantError and anything else: one readable line
            out_ << "Error: " << e.what() << "\n";
        }
    }
}

void MenuApp::optionDownload() {
    auto src = std::make_shared<BinanceDataSource>(http_, cfg_.baseUrls);
    DataCache cache(cfg_.dataDir, src);
    int ok = 0;
    for (const auto& sym : cfg_.symbols) {
        out_ << "Updating " << sym << " " << cfg_.interval << " (" << cfg_.historyDays << " days) ..." << std::endl;
        try {
            CandleSeries s = cache.update(sym, cfg_.interval, cfg_.historyDays);
            out_ << "  " << s << "\n";
            ++ok;
        } catch (const QuantError& e) {
            out_ << "  failed: " << e.what() << "\n";
        }
    }
    out_ << ok << " of " << cfg_.symbols.size() << " symbols updated.\n";
}

void MenuApp::optionSummary() {
    out_ << "\nData summary (" << cfg_.dataDir << ", interval " << cfg_.interval << "):\n";
    CsvDataSource csv(cfg_.dataDir);
    for (const auto& sym : cfg_.symbols) {
        if (!csv.exists(sym, cfg_.interval)) {
            out_ << "  " << std::left << std::setw(9) << sym << " no data (run option 1)\n";
            continue;
        }
        try {
            size_t gaps = 0;
            int64_t firstGap = -1;
            CandleSeries s = CsvDataSource::readFile(csv.pathFor(sym, cfg_.interval), sym, cfg_.interval, &gaps, &firstGap);
            out_ << "  " << std::left << std::setw(9) << sym << std::right << std::setw(7) << s.size() << " bars  "
                 << (s.empty() ? std::string("-") : formatUtc(s.front().openTime) + " .. " + formatUtc(s.back().openTime))
                 << "  last close " << (s.empty() ? 0.0 : s.back().close) << "  gaps " << gaps;
            if (gaps) out_ << " (first at " << formatUtc(firstGap) << ")";
            out_ << "\n";
        } catch (const QuantError& e) {
            out_ << "  " << sym << ": " << e.what() << "\n";
        }
    }
}

void MenuApp::optionBacktest() {
    std::vector<std::string> names = StrategyFactory::registered();
    const int si = pick("Strategy:", names);
    if (si < 0) return;
    const std::string name = names[static_cast<size_t>(si)];

    std::vector<std::string> universe;
    std::vector<std::string> options;
    if (StrategyFactory::isPairStrategy(name)) {
        for (const auto& p : cfg_.pairs) options.push_back(universeName(p));
        const int pi = pick("Pair (A/B):", options);
        if (pi < 0) return;
        universe = {cfg_.pairs[static_cast<size_t>(pi)].first, cfg_.pairs[static_cast<size_t>(pi)].second};
    } else {
        options = cfg_.symbols;
        const int yi = pick("Symbol:", options);
        if (yi < 0) return;
        universe = {cfg_.symbols[static_cast<size_t>(yi)]};
    }

    out_ << "Parameters as k=v;k=v (Enter for defaults). Grid keys: ";
    {
        std::unique_ptr<Strategy> probe = StrategyFactory::create(name);
        bool first = true;
        for (const auto& kv : probe->params().values()) {
            out_ << (first ? "" : ", ") << kv.first << "=" << kv.second;
            first = false;
        }
    }
    out_ << "\n> " << std::flush;
    std::string pstr;
    if (!readLine(pstr)) return;

    std::vector<CandleSeries> data = loadSeries(universe);
    const size_t dropped = alignSeries(data);
    if (dropped) out_ << "(aligned series, dropped " << dropped << " unmatched bars)\n";
    if (data[0].size() < 2) throw DataError("not enough data to backtest");

    auto cost = std::make_shared<BpsCostModel>(cfg_.feeBps, cfg_.slippageBps);
    std::unique_ptr<Strategy> strat = StrategyFactory::create(name);
    if (!pstr.empty()) strat->setParams(ParamSet::fromString(pstr));
    Backtester bt(std::move(strat), cost, cfg_.initialEquity);
    // Trained on the whole history, so this is an IN-SAMPLE number (option 4 does the honest test).
    bt.strategy().train(MarketView(data, 0, data[0].size() - 1));
    BacktestResult r = bt.run(data, 0, data[0].size());
    Backtester base(StrategyFactory::create("BuyAndHold"), cost, cfg_.initialEquity);
    BacktestResult hold = base.run(data, 0, data[0].size());

    out_ << "\n" << r << "\n" << hold << "\n"
         << "Fees " << cfg_.feeBps << " bps + slippage " << cfg_.slippageBps << " bps per side. Shorts have no borrow cost.\n"
         << "This is an in-sample backtest on the full history; use option 4 for walk-forward validation.\n";
}

void MenuApp::optionRecommend() {
    std::vector<CandleSeries> series = loadSeries(cfg_.symbols);
    auto cost = std::make_shared<BpsCostModel>(cfg_.feeBps, cfg_.slippageBps);
    StrategyRecommender rec(cfg_.walkForward(), cost, cfg_.minTradesOos);
    out_ << "Running walk-forward on " << series.size() << " symbols (this takes a minute or so) ...\n";
    RecommendationSet set = rec.rank(series, cfg_.pairs, [this](const std::string& m) { out_ << "  " << m << std::endl; });
    ReportWriter::printReport(out_, set);
    ensureDirectory(cfg_.reportDir);
    out_ << "Saved " << ReportWriter::saveReports(cfg_.reportDir, set, nowMs()) << " and "
         << cfg_.reportDir << "/latest_recommendation.csv\n";
}

void MenuApp::optionReport() {
    std::vector<Recommendation> rows = ReportWriter::readCsv(cfg_.reportDir + "/latest_recommendation.csv");
    RecommendationSet set;
    set.rows = rows;
    ReportWriter::printReport(out_, set);
}

void MenuApp::optionLive() {
    out_ << "Press Ctrl-C to stop live mode.\n";
    runLive(false, false);
}

int MenuApp::runLive(bool once, bool shadow) {
    try {
        std::vector<Recommendation> rows = ReportWriter::readCsv(cfg_.reportDir + "/latest_recommendation.csv");
        const Recommendation* choice = nullptr;
        for (const auto& r : rows)
            if (r.verdict == Verdict::Recommended && (!choice || r.rank < choice->rank)) choice = &r;
        bool recommended = true;
        if (!choice) {
            out_ << "No strategy has a RECOMMENDED verdict: none beat buy-and-hold out of sample on the last run.\n";
            for (const auto& r : rows)
                if (r.rank == 1) choice = &r;
            if (!choice) return 0;
            out_ << "Best-scoring row: " << choice->strategy << " on " << choice->universe << " (verdict "
                 << toString(choice->verdict) << ", NOT recommended).\n";
            if (!shadow) {
                if (once) return 0;  // non-interactive: do not guess, require --shadow
                out_ << "Monitor it in shadow mode anyway? [y/N] " << std::flush;
                std::string ans;
                if (!readLine(ans) || (ans != "y" && ans != "Y")) return 0;
            }
            recommended = false;
        }

        auto src = std::make_shared<BinanceDataSource>(http_, cfg_.baseUrls);
        LiveRunner runner(src, *choice, cfg_.interval, cfg_.wfTrain, cfg_.pollSeconds, recommended);
        runner.attach(std::make_shared<ConsolePrinter>(out_));
        ensureDirectory(cfg_.logDir);
        runner.attach(std::make_shared<FileLogger>(cfg_.logDir + "/live_signals.csv"));

        LiveRunner::clearStop();
        auto previous = std::signal(SIGINT, onSigint);
        try {
            runner.run(once);
        } catch (...) {
            std::signal(SIGINT, previous);
            throw;
        }
        std::signal(SIGINT, previous);
        if (LiveRunner::stopRequested()) out_ << "\nLive mode stopped.\n";
        LiveRunner::clearStop();
        return 0;
    } catch (const std::exception& e) {
        out_ << "Error: " << e.what() << "\n";
        return 1;
    }
}

int runCli(int argc, char** argv, std::shared_ptr<IHttpClient> http) {
    std::string configPath = "config.txt";
    bool live = false, once = false, shadow = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--live") live = true;
        else if (a == "--once") once = true;
        else if (a == "--shadow") shadow = true;
        else if (a == "--config" && i + 1 < argc) configPath = argv[++i];
        else if (a == "--help" || a == "-h") {
            std::cout << "usage: quant_engine [--config FILE] [--live [--once] [--shadow]]\n"
                         "  no flags: interactive menu\n"
                         "  --live    run live signals directly (Ctrl-C to stop)\n"
                         "  --once    with --live: one poll, then exit\n"
                         "  --shadow  with --live: allow a strategy that is not RECOMMENDED\n";
            return 0;
        } else {
            std::cerr << "unknown argument '" << a << "' (try --help)\n";
            return 2;
        }
    }
    try {
        Config cfg = Config::load(configPath);
        ensureDirectory(cfg.logDir);
        Logger::setLogFile(cfg.logDir + "/engine.log");
        MenuApp app(cfg, std::move(http), std::cin, std::cout);
        if (live) return app.runLive(once, shadow);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace quant
