#include "quant/live/LiveRunner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

#include "quant/core/Errors.hpp"
#include "quant/core/Logger.hpp"
#include "quant/core/TimeUtil.hpp"
#include "quant/strategies/StrategyFactory.hpp"

namespace quant {

namespace {
std::atomic<bool> gStop{false};
constexpr size_t kExtraBars = 200;  // history kept beyond the train window (SPEC section 9)
}  // namespace

void LiveRunner::requestStop() { gStop = true; }
bool LiveRunner::stopRequested() { return gStop.load(); }
void LiveRunner::clearStop() { gStop = false; }

LiveRunner::LiveRunner(std::shared_ptr<IDataSource> source, Recommendation choice, std::string interval,
                       size_t trainBars, int pollSeconds, bool recommended)
    : source_(std::move(source)),
      choice_(std::move(choice)),
      interval_(std::move(interval)),
      step_(intervalToMs(interval_)),
      trainBars_(trainBars),
      pollSeconds_(pollSeconds),
      recommended_(recommended),
      sleeper_([](int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }),
      clock_(nowMs) {
    if (!source_) throw ConfigError("LiveRunner needs a data source");
    std::istringstream ss(choice_.universe);
    std::string sym;
    while (std::getline(ss, sym, '/')) symbols_.push_back(sym);
    if (symbols_.empty()) throw ConfigError("LiveRunner: empty universe");
}

void LiveRunner::seed() {
    const size_t want = trainBars_ + kExtraBars;
    const int64_t now = clock_();
    data_.clear();
    for (const auto& sym : symbols_)
        data_.push_back(source_->fetchHistory(sym, interval_, now - static_cast<int64_t>(want + 10) * step_, now));
    alignSeries(data_);
    if (data_[0].size() < trainBars_)
        throw DataError("not enough history to train: have " + std::to_string(data_[0].size()) + " bars, need " +
                        std::to_string(trainBars_));

    strategy_ = StrategyFactory::create(choice_.strategy);
    strategy_->setParams(ParamSet::fromString(choice_.paramsLatest));
    const size_t n = data_[0].size();
    strategy_->train(MarketView(data_, n - trainBars_, n - 1));
    strategy_->reset();

    // Replay recent bars (no output) so indicator and position state match "now".
    const size_t replay = std::min(n, std::max<size_t>(strategy_->warmupBars() + 100, 300));
    for (size_t t = n - replay; t < n; ++t) current_ = strategy_->onBar(MarketView(data_, 0, t));
    currentText_ = describeTargets(current_);
    lastOpen_ = data_[0].back().openTime;
}

void LiveRunner::emitCurrent() {
    SignalEvent e;
    e.barOpenTime = lastOpen_;
    e.universe = choice_.universe;
    e.strategy = choice_.strategy;
    e.close = data_[0].back().close;
    e.weights = current_;
    e.changed = false;
    e.recommended = recommended_;
    for (auto& l : listeners_) l->onSignal(e);
}

void LiveRunner::processBar(size_t index) {
    const std::string before = currentText_;
    current_ = strategy_->onBar(MarketView(data_, 0, index));
    currentText_ = describeTargets(current_);
    lastOpen_ = data_[0][index].openTime;

    SignalEvent e;
    e.barOpenTime = lastOpen_;
    e.universe = choice_.universe;
    e.strategy = choice_.strategy;
    e.close = data_[0][index].close;
    e.weights = current_;
    e.previous = before;
    e.changed = before != currentText_;
    e.recommended = recommended_;
    for (auto& l : listeners_) l->onSignal(e);
}

bool LiveRunner::pollOnce() {
    if (!strategy_) throw ConfigError("LiveRunner::pollOnce called before seed()");
    // The newest bar every symbol has in common (they close together, but be defensive).
    int64_t newest = 0;
    for (size_t k = 0; k < symbols_.size(); ++k) {
        const Candle c = source_->fetchLatest(symbols_[k], interval_);
        newest = k == 0 ? c.openTime : std::min(newest, c.openTime);
    }
    if (newest <= lastOpen_) return false;

    // Fetch everything since the last processed bar (also repairs gaps after a network outage).
    std::vector<CandleSeries> fresh;
    for (const auto& sym : symbols_) fresh.push_back(source_->fetchHistory(sym, interval_, lastOpen_ + step_, newest));
    alignSeries(fresh);
    bool any = false;
    for (size_t i = 0; i < fresh[0].size(); ++i) {
        if (fresh[0][i].openTime <= lastOpen_) continue;
        for (size_t k = 0; k < data_.size(); ++k) data_[k].push_back(fresh[k][i]);
        processBar(data_[0].size() - 1);
        any = true;
    }
    return any;
}

void LiveRunner::waitSeconds(int s) const {
    for (int ms = 0; ms < s * 1000 && !stopRequested(); ms += 100) sleeper_(100);
}

void LiveRunner::run(bool once) {
    seed();
    emitCurrent();
    if (once) {
        pollOnce();
        return;
    }
    int failures = 0;
    while (!stopRequested()) {
        waitSeconds(pollSeconds_);
        if (stopRequested()) break;
        try {
            pollOnce();
            failures = 0;
        } catch (const NetworkError& e) {
            // Same exponential back-off as the data layer (1, 2, 4, 8, 16 s); keep running.
            const int wait = 1 << std::min(failures, 4);
            ++failures;
            Logger::warn(std::string("live poll failed (") + e.what() + "); retrying in " + std::to_string(wait) + " s");
            waitSeconds(wait);
        }
    }
}

}  // namespace quant
