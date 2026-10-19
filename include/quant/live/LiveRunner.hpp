#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "quant/data/IDataSource.hpp"
#include "quant/live/ISignalListener.hpp"
#include "quant/research/StrategyRecommender.hpp"
#include "quant/strategies/Strategy.hpp"

namespace quant {

// Signal display only. Never places orders and never needs an API key: it polls public candles,
// feeds each newly CLOSED candle to the chosen strategy and publishes the resulting target.
class LiveRunner {
public:
    LiveRunner(std::shared_ptr<IDataSource> source, Recommendation choice, std::string interval,
               size_t trainBars, int pollSeconds, bool recommended = true);

    void attach(std::shared_ptr<ISignalListener> listener) { listeners_.push_back(std::move(listener)); }
    void setSleeper(std::function<void(int ms)> s) { sleeper_ = std::move(s); }
    void setClock(std::function<int64_t()> c) { clock_ = std::move(c); }

    // Loads the last trainBars + 200 bars, trains on the latest train window with params_latest,
    // then replays recent bars so indicators and position state are current.
    void seed();
    // Publishes the strategy's current target (based on the latest closed bar).
    void emitCurrent();
    // One poll: if a new closed candle exists for every symbol, process it. Returns true if so.
    bool pollOnce();
    // seed + emitCurrent, then one poll (once) or poll until stop is requested (Ctrl-C).
    void run(bool once);

    const std::vector<TargetWeight>& currentTargets() const { return current_; }
    int64_t lastBarOpenTime() const { return lastOpen_; }

    // Ctrl-C support: a signal handler may only touch an atomic flag.
    static void requestStop();
    static bool stopRequested();
    static void clearStop();

private:
    void processBar(size_t index);
    void waitSeconds(int s) const;

    std::shared_ptr<IDataSource> source_;
    Recommendation choice_;
    std::string interval_;
    int64_t step_;
    size_t trainBars_;
    int pollSeconds_;
    bool recommended_;
    std::vector<std::string> symbols_;
    std::vector<CandleSeries> data_;
    std::unique_ptr<Strategy> strategy_;
    std::vector<TargetWeight> current_;
    std::string currentText_;
    int64_t lastOpen_ = 0;
    std::vector<std::shared_ptr<ISignalListener>> listeners_;
    std::function<void(int)> sleeper_;
    std::function<int64_t()> clock_;
};

}  // namespace quant
