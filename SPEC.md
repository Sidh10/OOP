# SPEC.md: Exact behaviour

Authoritative for formulas, formats, defaults, and tests. ARCHITECTURE.md is authoritative for class structure. If they conflict on behaviour, this file wins. Log any deviation in DECISIONS.md.

## 0. Defaults

Put these in one header, `include/quant/Defaults.hpp`, overridable from an optional `config.txt` (`key=value` lines, `#` comments).

| Setting | Default |
|---|---|
| Symbols | `BTCUSDT, ETHUSDT, BNBUSDT, SOLUSDT` |
| Pairs (A dependent / B regressor) | `ETHUSDT/BTCUSDT`, `SOLUSDT/ETHUSDT`, `BNBUSDT/ETHUSDT` |
| Interval | `1h` |
| History length | 730 days (about 17,520 bars) |
| Initial equity | 10000 (USDT) |
| Fee per side | 10 bps (0.10%); confirm current Binance spot fee, keep configurable |
| Slippage per side | 2 bps |
| Walk-forward | train 2000 bars, test 500 bars, step 500 |
| Min trades to select params in train | 10 |
| Min OOS trades for a verdict | 20 |
| Live poll interval | 30 s |
| Random seed (synthetic tests) | 42 |

## 1. Binance market data

Checked against the official Spot REST docs on 2026-10-06.

- Endpoint: `GET {base}/api/v3/klines`.
- Query params: `symbol` (required), `interval` (required, case-sensitive), `startTime` and `endTime` (ms since epoch, UTC, inclusive), `limit` (default 500, **max 1000**). Do not use `timeZone`.
- Valid intervals: `1s 1m 3m 5m 15m 30m 1h 2h 4h 6h 8h 12h 1d 3d 1w 1M`.
- No `startTime`/`endTime` returns the most recent klines. Klines are uniquely identified by open time.
- Response: JSON array of 12-element arrays. Use indexes 0 to 6 only:

| idx | field | JSON type |
|---|---|---|
| 0 | open time (ms) | integer |
| 1 | open | **string** |
| 2 | high | string |
| 3 | low | string |
| 4 | close | string |
| 5 | volume (base asset) | string |
| 6 | close time (ms) | integer |
| 7-11 | quote volume, trade count, taker volumes, ignore | ignore |

- Convert price/volume strings to `double` with `std::strtod` (locale-independent for `.` decimals). Reject a row that is not an array of length 12 with `ParseError`.
- Base URLs (all listed in the docs). Try in this order and fall through on connection failure, HTTP 451 or 403: `https://api.binance.com`, `https://data-api.binance.vision` (market data only), `https://api1.binance.com`. The base list is configurable.
- Request weight for klines is 2 per request (IP-based). Sleep 250 ms between pages.
- HTTP 429 means rate-limited: honour `Retry-After` if present, otherwise exponential back-off (1, 2, 4, 8, 16 s), at most 5 tries. HTTP 418 means the IP is banned: stop immediately and report. These two codes are standard Binance behaviour, not re-verified from the page checked above.
- Paging for history: `cur = fromMs`; loop: request `limit=1000, startTime=cur, endTime=toMs`; append; `cur = lastOpenTime + intervalMs`; stop when fewer than 1000 rows came back or `cur > toMs`.
- Drop any candle whose `closeTime >= now` (still forming). Deduplicate by open time. Sort ascending.
- Interval to milliseconds: `1m=60000, 5m=300000, 15m=900000, 30m=1800000, 1h=3600000, 4h=14400000, 1d=86400000` (and so on for the rest).
- Only public `GET` endpoints. Never send API keys. Live latest candle: `klines?symbol=S&interval=I&limit=2`, take the last row with `closeTime < now`.

## 2. CSV cache

- File: `data/{SYMBOL}_{interval}.csv`, header `open_time,open,high,low,close,volume,close_time`.
- `open_time` and `close_time` are integer ms. Prices and volume written with `%.8f`. Ascending, unique.
- Incremental update: read the last `open_time`, fetch from `last + intervalMs` to now, append.
- Validation on load: strictly increasing, `high >= max(open, close)`, `low <= min(open, close)`, all prices > 0. Count gaps (`diff != intervalMs`) and log a warning with the count and the first gap. A gap is not fatal.
- Corrupt file: throw `DataError` naming the file and row number. Never silently skip rows.

## 3. Core types

- `Candle { int64_t openTime, closeTime; double open, high, low, close, volume; }`
- `CandleSeries`: symbol, interval string, `std::vector<Candle>`. `operator[]` unchecked, `at()` checked (`DataError`), `slice(from, to)` half-open, iterators, `operator<<`.
- `alignSeries(vector<CandleSeries>&)`: inner join on `openTime` so all series have the same length and timestamps. Log how many bars were dropped.
- `RollingWindow<T>`: fixed capacity, O(1) push, `mean()`, sample `variance()`, `stddev()`. Must stay numerically stable over 100k pushes (test against a naive recompute).
- Errors: `QuantError : std::runtime_error`, with `NetworkError`, `ParseError`, `DataError`, `ConfigError`.

## 4. Indicators

All return `NaN` until warmed up. Each has `ready()`.

- **SMA(n)**: mean of last n values.
- **EMA(n)**: `alpha = 2/(n+1)`, seeded with the SMA of the first n values.
- **RSI(n)**: Wilder smoothing, 0 to 100.
- **ZScore(n)**: `(x - mean_n) / std_n` with sample std. Return 0 if `std_n < 1e-12`.
- **ATR(n)**: Wilder smoothing of true range.

## 5. Models

- **RollingOLS(W)**: `y = a + b*x` over the last W points, closed form. Exposes alpha, beta, residual std. If the regressor variance is below 1e-12, keep the previous estimate.
- **KalmanFilter**: state `[alpha, beta]` as a random walk, observation `y_t = alpha + beta*x_t + noise`. Parameters: state noise `delta = 1e-5`, observation noise `Ve = 1e-3` (both configurable). Exposes the innovation `e_t` and its standard deviation `sqrt(Q_t)`.
- **LogisticRegression**: binary, standardised inputs, batch gradient descent for `epochs` (default 50), L2 penalty, plus `update(x, y)` for one online SGD step. Standardisation statistics come from the training window only.
- **ADF diagnostic** (pairs only, report-only): Dickey-Fuller statistic on the spread residual with a constant. It is shown in the report as a diagnostic and never gates trading. If you cite critical values (Engle-Granger / MacKinnon, roughly -3.34 at 5% for two variables with a constant), state the source and mark them approximate.

## 6. Strategies

Every strategy decides using information up to and including the close of bar `t`, and returns target weights for the next bar.

Common params: `longShort` (0 or 1, default 0). Single-asset strategies output weight in `{0, +1}`. With `longShort=1`, a bearish signal gives `-1`.

| Strategy | Logic | Param grid |
|---|---|---|
| `BuyAndHold` | weight 1 from the first bar | none (baseline, never a candidate) |
| `MeanReversion` | `z = ZScore(window)` on close. Flat and `z < -entryZ` goes long. Long and `z > -exitZ` goes flat. | window {20,50,100}, entryZ {1.5,2.0,2.5}, exitZ {0,0.5} |
| `Momentum` | long when `SMA(fast) > SMA(slow)`, else flat | fast {5,10,20,50}, slow {30,50,100,200}, require fast < slow |
| `Breakout` | long when close > highest high of the previous N bars (excluding the current one). Flat when close < lowest low of the previous M bars. | N {20,50,100}, M {10,20,50}, require M < N |
| `PairsTrading` | see below | window {60,120,240}, entryZ {1.5,2.0,2.5}, exitZ {0,0.5}, stopZ {4}, hedge {0=OLS, 1=Kalman} |
| `MLSignal` | see below | lr {0.01,0.05}, l2 {0,0.001}, threshold {0.52,0.55,0.6}, online {0,1} |

**PairsTrading** (A dependent, B regressor, on natural-log prices):
- Hedge: `ln A = alpha + beta * ln B` by `RollingOLS(window)` or by `KalmanFilter`. Spread `s = ln A - (alpha + beta*ln B)`. `z` is `(s - mean)/std` over the window for OLS, or the standardised Kalman innovation for Kalman.
- If `beta <= 0`, stay flat (documented limitation).
- State `pos` in {-1, 0, +1}, where +1 means long the spread (long A, short B). Enter `+1` when `z < -entryZ`, `-1` when `z > entryZ`. Exit when `|z| < exitZ`. Stop out to flat when `|z| > stopZ`, then wait `window/2` bars before re-entering.
- Weights (gross exposure 1): `wA = pos / (1 + |beta|)`, `wB = -pos * beta / (1 + |beta|)`.

**MLSignal** (single asset):
- Features at bar t: 1-bar log return, 5-bar log return, 20-bar log return, `(RSI14 - 50)/50`, `ZScore20` of close, 20-bar std of 1-bar log returns.
- Label `y_t = 1` if `close_{t+1} > close_t`, else 0. A training pair `(x_t, y_t)` is only usable once bar `t+1` exists in the training window.
- Long if `P(up) > threshold`, else flat. With `online=1`, at bar t first update the model with the now-known pair `(x_{t-1}, y_{t-1})`, then predict. Nothing else may use future information.

Strategy lifecycle (see ARCHITECTURE.md): `clone()` per candidate, `train(view)` once on the train window, then `onBar(view)` for each bar. State is reset before every run. Incremental indicator state or recomputing from `history()` are both fine, as long as only bars `<= t` are read.

Grid sizes must stay small enough that the full recommendation run finishes in about 5 minutes or less in Release mode on a normal laptop. If not, trim grids and log it.

## 7. Backtester semantics

All series aligned (`alignSeries`), N bars. For `t = 0 .. N-1`:

1. Execute any pending target weights (decided at the close of bar `t-1`) at the **open** of bar `t`. Fill price `= open * (1 + slip * sign(dV))`. Fee `= feeRate * |dV|`, where `dV` is the trade notional.
2. Mark the portfolio at the **close** of bar `t` and record `equity[t]`.
3. Build `MarketView` at index `t`, call `strategy.onBar`, store the result as pending for `t+1`. The last bar's decision is never executed.

Sizing:
- Desired position value `V* = w * equity` (equity marked at the bar-t open). `dV = V* - V_current`.
- Spot constraint: if buying, scale `dV` so `dV * (1 + feeRate + slip) <= cash`. Cash never goes negative.
- Gross exposure `sum |w| <= 1`. Scale down if exceeded.
- Shorts are simulated at the fill price with **no borrow or funding cost**. State this limitation in the report output and README.

Trades: a trade is an episode where gross exposure goes from flat to non-flat and back to flat. `numTrades` counts episodes. `winRate` is the fraction of episodes with positive net P&L (after fees).

Metrics (`PerformanceMetrics`), from the equity curve:
- Bar return `r_t = equity[t]/equity[t-1] - 1`.
- `periodsPerYear = 365 * 86400000 / intervalMs` (crypto trades 24/7).
- `Sharpe = mean(r) / std_sample(r) * sqrt(periodsPerYear)`, risk-free rate 0, and 0 if `std < 1e-12`.
- `Sortino = mean(r) / downsideDev * sqrt(periodsPerYear)`, `downsideDev = sqrt(mean(min(r,0)^2))`.
- `maxDrawdown = max_t (peak_t - equity_t) / peak_t`, in [0, 1].
- `CAGR = (equity_end/equity_start)^(periodsPerYear / nBars) - 1`.
- Also: `totalReturn`, `numTrades`, `winRate`, `exposure` (mean gross weight), `turnover` (sum |dV| / mean equity).

## 8. Walk-forward and recommender

Windows over N bars, rolling: `s = 0`; train `[s, s+T)`, test `[s+T, s+T+E)`; `s += E`; repeat while the test window fits.

Per window and strategy:
1. For every param combination: clone, train on the train window, backtest on the train window, compute in-sample metrics. Choose the combination with the highest in-sample Sharpe among those with `numTrades >= minTradesTrain`. If none qualify, the window is skipped for that strategy.
2. Run the chosen combination on the test window. The strategy may read bars before the test start (they are the past) but must start flat with initial equity 1. Record the per-bar test returns and trades.
3. Concatenate the test-window returns into one stitched out-of-sample series. Each window starts flat, which is conservative, because entry costs are paid again.

Baseline: `BuyAndHold` on exactly the same stitched out-of-sample bars.

Per (universe, strategy), where universe is a single symbol or a pair:
- `oosSharpe`, `oosSortino`, `oosCAGR`, `oosMaxDD`, `oosTrades`, `oosWinRate` from the stitched series.
- `isSharpe` = mean of the selected in-sample Sharpe values. `overfitGap = isSharpe - oosSharpe`.
- `score = oosSharpe - 1.0 * oosMaxDD - 0.5 * max(0, overfitGap)`.

Verdict (thresholds in `Defaults.hpp`, not to be changed after seeing results):
- `INSUFFICIENT` if `oosTrades < 20`.
- `RECOMMENDED` if `oosSharpe > baselineSharpe + 0.2` and `oosSharpe > 0.5` and `overfitGap <= 1.0`.
- `WEAK` if `oosSharpe > 0` but not RECOMMENDED.
- `REJECT` otherwise.

Rank by `score` descending among non-INSUFFICIENT entries. If nothing is RECOMMENDED, print this plainly: no strategy beat the buy-and-hold baseline out of sample on this data.

Multiple-testing caveat: with grids and many universes, some winners are luck. State this in the report text.

Report outputs:
- `reports/recommendation_YYYYMMDD_HHMMSS.csv` and an identical `reports/latest_recommendation.csv`, columns: `rank,universe,strategy,verdict,score,oos_sharpe,oos_sortino,oos_cagr,oos_maxdd,oos_trades,oos_winrate,is_sharpe,overfit_gap,baseline_sharpe,baseline_cagr,params_latest,n_windows,period_start,period_end`. `params_latest` is `k=v;k=v` from the last window's selection.
- A console table of the same.
- Always include the line: `Research output, not financial advice. Past performance does not predict future results.`

## 9. Live mode

- Load `reports/latest_recommendation.csv`. Use the top-ranked `RECOMMENDED` row. If there is none, say so, and offer to monitor the best-scoring row in shadow mode, clearly labelled **not recommended**. If the file is missing, tell the user to run option 4 first.
- Seed: load the last `trainBars + 200` bars from cache or the API, train the strategy on the latest train window with `params_latest`.
- Loop: every `pollSeconds`, request the latest klines. When a new closed candle appears (a new `openTime`), append it, call `onBar`, print and log the result.
- Console line: `2026-10-06 18:00Z BTCUSDT close=... strategy=... target=LONG(1.00) [changed from FLAT]`.
- Log to `logs/live_signals.csv`: `timestamp,universe,strategy,close,target_weights,changed`.
- `--once` flag: one poll then exit (for tests).
- Handle Ctrl-C cleanly (`std::atomic<bool>` set from a signal handler). On network errors, retry with the same back-off as SPEC §1 and keep running.
- Never place orders. Never ask for or store API keys.

## 10. CLI menu

```
1) Download / update data
2) Show data summary
3) Backtest one strategy
4) Run walk-forward + recommend strategies
5) Live signals
6) Show last recommendation report
0) Exit
```

Invalid input must not crash it. End of input (EOF) exits cleanly, so `printf "2\n0\n" | build/quant_engine.exe` works as a smoke test. Exceptions are caught at menu level and shown as one readable line. Disclaimer line printed on startup.

## 11. Logging

Small `Logger` class (levels INFO/WARN/ERROR) writing to stderr and `logs/engine.log`. No logging inside inner backtest loops.

## 12. Required tests

All in `tests/`, run by `ctest`, using `tests/minitest.hpp`. Each must exist and pass. Add more freely.

1. **Kline parsing**: `tests/fixtures/klines_sample.json` (synthetic, in the exact 12-field layout from §1, at least 3 rows) parses to the expected doubles. A malformed row throws `ParseError`.
2. **Paging**: `MockHttpClient` returns two pages (1000 rows, then 37). `fetchHistory` returns 1037 rows, ascending, no duplicates, and requests used the right `startTime` progression.
3. **Unclosed candle dropped**: a mock response whose last row has a future `closeTime` is excluded.
4. **CSV round trip** and incremental append, plus a corrupt-file error naming file and row.
5. **Indicators** match a naive reference implementation inside the test (SMA, EMA, RSI, ZScore, ATR). Hand check: `SMA(3)` of `[1,2,3,4,5]` is `[NaN, NaN, 2, 3, 4]`.
6. **RollingWindow** matches naive mean/variance after 100,000 pushes (tolerance 1e-9 relative).
7. **Backtester toy dataset** (zero costs, initial equity 10000, an always-long strategy decided at the close of bar 0):

   | bar | open | high | low | close |
   |---|---|---|---|---|
   | 0 | 100 | 101 | 99 | 100 |
   | 1 | 100 | 111 | 99 | 110 |
   | 2 | 110 | 122 | 109 | 121 |
   | 3 | 121 | 122 | 100 | 110 |

   Fill at bar 1 open 100 gives 100 units. Equity: close0 = 10000, close1 = 11000, close2 = 12100, close3 = 11000. Bar returns `0.1, 0.1, -0.090909...`. `maxDrawdown = 1100/12100 = 0.0909090909`. `totalReturn = 0.10`. Tolerance 1e-9.
8. **Cost check** on the same data with fee 10 bps, slippage 0: cash cannot go negative, so `dV = 10000/1.001 = 9990.00999`, and equity at close of bar 1 is `9990.009990 * 1.1 = 10989.010989` (tolerance 1e-6). Write the hand calculation in a comment.
9. **No-lookahead (leakage) test**, for every registered strategy: run on series S, and again on S' where all bars after index k are replaced by random garbage. Target weights for bars `<= k` must be identical.
10. **Execution timing**: a strategy that flips signal on bar t must see its fill at the open of bar t+1, never at bar t.
11. **Metrics** vs hand-computed values on a tiny equity curve (Sharpe, Sortino, max drawdown, CAGR).
12. **Models**: `RollingOLS` recovers known alpha/beta on synthetic data; `KalmanFilter` beta converges near the true beta on a synthetic pair; `LogisticRegression` separates a synthetic linearly-separable set (accuracy > 0.95).
13. **Pairs on a synthetic cointegrated pair** (fixed seed 42): the pairs strategy has positive out-of-sample return after costs.
14. **Honesty test**: on a seeded random-walk series (seed 42), the recommender must not label any strategy `RECOMMENDED`. If this test is flaky because of the chosen seed or thresholds, do not loosen the thresholds. Investigate whether there is leakage and log the outcome in DECISIONS.md.
15. **Walk-forward windows**: window boundaries are correct, test windows never overlap, and no train bar is inside a test window.
16. **Menu smoke test**: piped input `2`, `0` exits with code 0.

## 13. Performance and numerics

- Release build, `-O2`. Use incremental indicator updates, not O(n * window) recomputation per bar.
- Doubles only. Guard divisions and logs against zero and negative inputs.
