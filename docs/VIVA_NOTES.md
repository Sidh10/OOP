# Viva notes

Written so you can explain it yourself. File paths are relative to the project folder; headers are in
`include/quant/...`, sources in `src/...`.

## Part 1: OOP concept map (where each concept really is used)

| Concept | Where (file and class) | Two-sentence justification |
|---|---|---|
| Abstraction / interfaces | `data/IHttpClient.hpp` `IHttpClient`; `data/IDataSource.hpp` `IDataSource`; `engine/CostModel.hpp` `CostModel`; `live/ISignalListener.hpp` `ISignalListener` | Each is a pure abstract class that says *what* is needed (fetch a URL, give me candles, price a trade, receive a signal) and hides *how*. Callers such as `BinanceDataSource`, `LiveRunner` and `Portfolio` depend only on the interface, so any implementation can be plugged in. |
| Inheritance + polymorphism | `strategies/Strategy.hpp` `Strategy` with `BuyAndHoldStrategy`, `MeanReversionStrategy`, `MomentumStrategy`, `BreakoutStrategy` (`SimpleStrategies.hpp`), `PairsTradingStrategy`, `MLSignalStrategy` (`AdvancedStrategies.hpp`); `indicators/Indicator.hpp` `Indicator` -> `PriceIndicator` -> `SMA`, `EMA`, `RSI`, `ZScore`, and `ATR`; `models/Model.hpp` `Model` -> `RollingOLS`, `KalmanFilter`, `LogisticRegression` | The `Backtester` holds a `std::unique_ptr<Strategy>` and calls `onBar()` without knowing which strategy it is; the virtual call picks the right code at run time. The same holds for `Indicator*` and `Model*` (see `test_indicators.cpp`, `test_models.cpp`). |
| Encapsulation | `engine/Portfolio.hpp` `Portfolio` | Cash and positions are private; the only way to change them is `rebalance()`, which enforces the rules (sells first, cash never negative, gross exposure at most 1, fees and slippage). A bug in a strategy therefore cannot create money from nothing. |
| Composition over inheritance | `MeanReversionStrategy` owns a `ZScore`; `MomentumStrategy` owns two `SMA`; `PairsTradingStrategy` owns a `RollingOLS`, a `KalmanFilter` and a `RollingWindow`; `MLSignalStrategy` owns a `LogisticRegression` and an `MLFeatureBuilder`; `BinanceDataSource` owns a `shared_ptr<IHttpClient>`; `Backtester` owns a `Strategy` and a `CostModel` | A strategy *has* an indicator, it *is not* one, so "MeanReversion inherits ZScore" would be wrong. Composition also lets a strategy swap its OLS or Kalman helper by a parameter without changing its class. |
| Templates | `core/RollingWindow.hpp` `RollingWindow<T>` | One fixed-size ring buffer with running mean and variance, reused for closes, spreads, returns and highs/lows. The type parameter keeps it generic without copy-pasting it per type. |
| Operator overloading | `CandleSeries::operator[]`; `operator<<` for `Candle`, `CandleSeries` (`core/CandleSeries.cpp`) and `BacktestResult` (`engine/BacktestResult.cpp`); `ParamSet::operator==` | `series[i]` reads like an array while the class stays encapsulated, and `std::cout << result` prints a whole backtest summary. Both make calling code short and readable. |
| Smart pointers / RAII | `unique_ptr<Strategy>` in `Backtester` and `StrategyFactory::create`; `shared_ptr<IHttpClient>` in `BinanceDataSource`/`MenuApp`; `data/CurlHttpClient.cpp` `CurlHttpClient` | Ownership is explicit and there is no `new`/`delete` anywhere. `CurlHttpClient` acquires the curl handle in its constructor and releases it in its destructor, so it is freed even when `get()` throws. |
| Exceptions | `core/Errors.hpp`: `QuantError` -> `NetworkError`, `ParseError`, `DataError`, `ConfigError` | One root type lets `MenuApp` catch everything in one place and print one readable line. `NetworkError` also carries the HTTP status and `Retry-After`, which the retry logic uses. |
| STL | `vector` everywhere; `map` in `ParamSet`; `deque` in `MockHttpClient`; `<algorithm>` (`sort`, `unique`, `remove_if`, `set_intersection` in `alignSeries`); `<numeric>` (`accumulate` in `PerformanceMetrics`); `std::function` for injected clock/sleeper; `std::optional` for strategy members | Standard containers and algorithms replace hand-written loops where they fit, which makes the code shorter and less error-prone. Note: `RollingWindow` is a vector ring buffer, not a `deque` as the early plan said. |
| File handling | `data/CsvDataSource.cpp` and `DataCache.cpp` (CSV cache), `research/ReportWriter.cpp` (report CSV), `live/Listeners.cpp` `FileLogger`, `core/Logger.cpp`, `Config.cpp` (`config.txt`) | Downloaded data is cached so the program does not hit the API on every run, and results are saved as CSV that Excel can open. Reading a bad file throws a `DataError` that names the file and the row. |
| Factory pattern | `strategies/StrategyFactory.cpp` `StrategyFactory::create/registered/candidates/grid` | Research and menu code create strategies from a name and read each one's parameter grid without including concrete classes. Adding a strategy means one new class and one table entry. |
| Strategy pattern | `Strategy` hierarchy used by `Backtester`, `WalkForwardOptimizer`, `LiveRunner` | The algorithm (what to hold next) is a replaceable object, while the loop that runs it (timing, costs, metrics) stays the same. |
| Observer pattern | `live/LiveRunner.cpp` publishes `SignalEvent` to `ISignalListener`s: `ConsolePrinter`, `FileLogger` | The runner does not know who consumes a signal; printing and file logging are two independent listeners attached with `attach()`. A third (say, a sound alert) could be added without touching the runner. |
| Dependency injection | `BinanceDataSource(shared_ptr<IHttpClient>)`, `MenuApp(cfg, http, istream&, ostream&)`, injected clock and sleeper in `BinanceDataSource` and `LiveRunner` | Dependencies come in through the constructor, so tests pass a `MockHttpClient`, a string stream and a fake clock. That is how the paging, 429 back-off and menu tests run in milliseconds with no network. |

## Part 2: Likely viva questions, with answers about this code

**1. Why composition for strategies and indicators instead of inheritance?**
"Is-a" has to be true. A mean-reversion strategy is not a z-score; it *uses* one. In `MeanReversionStrategy` the `ZScore` is a private member (`std::optional<ZScore> z_`), rebuilt in `applyParams()` when the window parameter changes. Inheritance would have tied the strategy to one indicator and made `PairsTrading` (which needs OLS *or* Kalman) impossible to express cleanly.

**2. Why does `Portfolio` own all the state?**
So rules live in one place. `Portfolio::rebalance` (`src/engine/Portfolio.cpp`) is the only code that changes cash and units: it executes sells first, scales buys so `dV * (1 + fee + slippage) <= cash`, scales gross exposure to at most 1, and charges fees. Strategies only return *target weights*; they never touch money.

**3. How does the code prevent lookahead bias?**
Three layers. (a) Strategies only receive a `MarketView` (`strategies/MarketView.hpp`) that ends at the current bar; there is no way to ask for bar t+1. (b) The `Backtester` loop (`src/engine/Backtester.cpp`) executes a decision made at the close of bar t at the *open* of bar t+1, and never executes the last bar's decision. (c) Tests: `no_lookahead_for_every_registered_strategy` replaces all bars after index k with random garbage and requires identical weights up to k for every registered strategy; a second test uses a deliberately cheating strategy to show the test would catch a leak.

**4. What does walk-forward do and why not just optimise on all the data?**
`WalkForwardOptimizer::run` slides a window: train on 2000 bars, pick the parameter set with the best in-sample Sharpe (at least 10 trades), then run only that set on the next 500 *unseen* bars, and move forward by 500. The test pieces are stitched into one out-of-sample curve. Optimising on all data would just find the parameters that fit the past best, which says nothing about the future.

**5. What is the Sharpe ratio formula used here?**
`Sharpe = mean(r) / std_sample(r) * sqrt(periodsPerYear)` with risk-free rate 0, where `r` are the bar-to-bar equity returns and `periodsPerYear = 365 * 86400000 / intervalMs` (8760 for 1-hour bars, because crypto trades 24/7). It is in `PerformanceMetrics` (`src/engine/PerformanceMetrics.cpp`) and checked against a hand-computed value in `test_engine.cpp`. Sortino divides by downside deviation instead of the standard deviation; max drawdown is the largest peak-to-trough fall of the equity curve.

**6. Why is there a `MockHttpClient`?**
Tests must not depend on the internet, be repeatable, and be able to produce failures on demand. `MockHttpClient` implements the same `IHttpClient` interface and replays queued pages or HTTP errors, which lets `test_data.cpp` check paging (1000 + 37 rows), dropping the unclosed candle, HTTP 429 back-off and base-URL fallback exactly.

**7. What does RAII protect?**
Resources are tied to object lifetime. `CurlHttpClient` creates the curl handle in its constructor and cleans it in its destructor; `std::ofstream`, `unique_ptr` and `shared_ptr` do the same for files and memory. Nothing leaks even when an exception is thrown halfway through a download.

**8. Why does `Strategy` have `clone()` and `reset()`?**
Every candidate parameter set needs its own independent state. `WalkForwardOptimizer` calls `proto->clone()` for each grid point; `Backtester::run` calls `reset()` first, which clears streaming state (positions, indicator buffers) but keeps parameters and what was learned in `train()`, so a strategy trained once can be run on the train window and then the test window.

**9. Why a template `RollingWindow<T>`, and why does it recompute every `capacity` pushes?**
It is a ring buffer with O(1) push and running mean/variance. Adding and removing values from running sums slowly drifts in floating point, so every `capacity` pushes it recomputes the statistics exactly, which keeps it within 1e-9 of a naive recomputation after 100,000 pushes (`test_core.cpp`).

**10. How are errors handled?**
All errors derive from `QuantError`: `NetworkError` (with HTTP status), `ParseError`, `DataError`, `ConfigError`. Library code throws; `MenuApp::run` catches at menu level and prints one line, so a bad file or a network failure never crashes the menu. A corrupt CSV names the file and row.

**11. How are costs and shorts modelled?**
`BpsCostModel` charges 10 bps fee plus 2 bps slippage per side: buys fill at `open * (1 + slippage)`, sells at `open * (1 - slippage)`. Shorts are simulated at the fill price with no borrow or funding cost, which flatters short-side results and is stated in the report and README.

**12. What does the factory give you, and how would you add a new strategy?**
Create a class deriving from `Strategy` (name, `onBar`, `reset`, `clone`, `applyParams`), then add one entry with its parameter grid in `StrategyFactory.cpp`. The walk-forward, recommender, menu and the leakage test pick it up automatically because they use `registered()`.

**13. What happens when Binance returns 429, 418 or 451?**
In `BinanceDataSource::request`: 429 waits `Retry-After` or 1, 2, 4, 8 seconds and retries (5 tries); 418 (IP ban) stops immediately; 451, 403 or a connection failure falls through to the next base URL (`api.binance.com`, `data-api.binance.vision`, `api1.binance.com`). The sleep is injectable, so the tests do not really wait.

**14. Why do all rows say REJECT, and can the thresholds be changed?**
On the real 2-year data no strategy beat buy-and-hold out of sample once 12 bps of costs per side are paid; the report prints that plainly. `RECOMMENDED` needs OOS Sharpe above both 0.5 and baseline + 0.2, at least 20 OOS trades, and an in-sample/out-of-sample gap of at most 1; these are fixed in `Defaults.hpp` *before* looking at results, because tuning them afterwards would be cheating. With many strategies tested, some luck is expected (on random walks about 5% of rows pass), which is why the report carries a multiple-testing caveat.

**15. How does live mode work, and how does Ctrl-C stop it?**
`LiveRunner` seeds the last 2200 bars, trains the recommended strategy on the latest 2000, replays recent bars, then polls the latest klines every 30 s and processes each newly *closed* candle (also catching up on missed ones). Results go to `ConsolePrinter` and `FileLogger` through the Observer interface. Ctrl-C calls a signal handler that only sets a `std::atomic<bool>`; the loop checks it and exits cleanly.

**16. (Bonus) How does the pairs strategy work?**
It regresses `ln A` on `ln B` (rolling OLS, or a Kalman filter that lets the hedge ratio drift), takes the residual spread, and standardises it into a z-score. It goes long the spread (long A, short B, weights `1/(1+beta)` and `-beta/(1+beta)`) when z is below `-entryZ`, short when above `entryZ`, exits near zero, and stops out beyond `stopZ`. The Dickey-Fuller statistic is shown only as a diagnostic: on this data it never reached the roughly -3.34 level, i.e. cointegration is not supported.

**17. (Bonus) What is "online" in the ML strategy?**
With `online=1`, at each new bar the model first learns from the now-known outcome of the previous bar (`update(x_{t-1}, y_{t-1})`), then predicts for the next bar. Training uses only the train window, and standardisation statistics come from that window only, so no future information leaks in.
