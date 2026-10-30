# AI Quant Trading & Statistical Arbitrage Engine

C++17 OOP course project (VIT Pune). A command-line program that

1. downloads real historical candles from the Binance public market-data API (no API key),
2. trains and **walk-forward validates** a small library of trading strategies on them, with fees and slippage,
3. ranks the strategies against a buy-and-hold baseline, and
4. shows live signals from fresh closed candles.

It is **backtesting and signal display only**. It never places orders and never asks for keys.
*Research output, not financial advice. Past performance does not predict future results.*

## Quick start (double-click)

Double-click `run_quant.bat` in the project folder to open the menu; the window stays open after you exit. It can also
be run from any folder or with options, for example `run_quant.bat --live --once --shadow`. The build commands below are
only needed the first time and after the C++ code changes; if `build\quant_engine.exe` is missing the launcher says so.

## Build and run (Windows, MSYS2 UCRT64)

One-time setup in the MSYS2 UCRT64 shell:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-ninja
```

Then, from the project folder:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
build/quant_engine.exe
```

Third-party code: `nlohmann/json` (single header, vendored in `third_party/`) and libcurl. HTTPS certificate
verification is always on. If the program cannot find a CA bundle outside the MSYS2 shell, set
`CURL_CA_BUNDLE` to a `ca-bundle.crt` file; it is never disabled.

## The menu

```
1) Download / update data           4) Run walk-forward + recommend strategies
2) Show data summary                5) Live signals
3) Backtest one strategy            6) Show last recommendation report
0) Exit
```

Typical first run: **1** (about a minute for 4 symbols x 730 days), then **4** (about 30 seconds), then **6** or **5**.

* **3** is an in-sample backtest on the whole history: handy for looking at one strategy, not evidence that it works.
* **4** is the honest test: parameters are chosen on a train window and scored only on the following unseen window.
  Output goes to `reports/recommendation_<timestamp>.csv` and `reports/latest_recommendation.csv`.
* **5** uses the top `RECOMMENDED` row. If nothing earned that verdict it says so and only monitors the best row
  in clearly labelled shadow mode, if you agree. Ctrl-C stops it. Signals are also appended to `logs/live_signals.csv`.

Flags: `--live [--once] [--shadow]` runs option 5 directly (`--once` polls once and exits), `--config FILE`, `--help`.
End of input exits cleanly, so `printf "2\n0\n" | build/quant_engine.exe` works as a smoke test.

## Settings

Defaults live in `include/quant/Defaults.hpp`. An optional `config.txt` (`key=value`, `#` comments) overrides them:
`symbols`, `pairs` (`ETHUSDT/BTCUSDT,...`), `interval`, `history_days`, `initial_equity`, `fee_bps`, `slippage_bps`,
`wf_train`, `wf_test`, `wf_step`, `min_trades_train`, `min_trades_oos`, `poll_seconds`, `base_urls`, `data_dir`,
`report_dir`, `log_dir`. A bad line stops the program with the file name and line number. The verdict thresholds are
fixed in code on purpose, so they cannot be tuned after seeing results.

## What is inside

| Folder | Contents |
|---|---|
| `include/quant`, `src` | `core` (candles, errors, rolling window), `data` (HTTP, Binance, CSV cache), `indicators`, `models` (rolling OLS, Kalman, logistic regression), `strategies`, `engine` (portfolio, backtester, metrics), `research` (walk-forward, recommender, report), `live`, `ui` |
| `tests` | own mini test harness and 80+ tests, including the no-lookahead test for every strategy |
| `docs/VIVA_NOTES.md` | where each OOP concept is used, plus likely viva questions |
| `DECISIONS.md` | every non-obvious choice and why |

Strategies: BuyAndHold (baseline), MeanReversion, Momentum, Breakout, PairsTrading (rolling OLS or Kalman hedge ratio),
MLSignal (logistic regression, optionally updated online).

## Showcase page

`site/index.html` is a one-page presentation of the project and its result (open the file in a browser; it needs no
server). It is a static page, separate from the engine: the CLI stays the only way to run anything. The numbers on it
come straight from `reports/latest_recommendation.csv`; after a new run of menu option 4, refresh them with
`node site/build.mjs` (Node 22 or newer).

## Honest limits

* Backtests overfit. Walk-forward reduces this but does not remove it, and with many strategies, parameters and
  universes some "winners" are luck (multiple testing). On pure random walks about 5% of rows still pass the verdict rule.
* Costs matter: 10 bps fee + 2 bps slippage per side are assumed (Binance spot is about 0.1% per side; check your tier).
* Shorts are simulated at the fill price with **no borrow or funding cost**, so short-side results flatter reality.
* Crypto history is short and regime-dependent. The last real run (2 years, 4 symbols, 3 pairs) found **no strategy
  that beat buy-and-hold out of sample**, and the program is built to be able to say exactly that.
* Live mode shows signals for the last closed candle only; it does not trade and is not a trading system.

## Verification status

Verified on 2026-10-18 with MSYS2 UCRT64 (g++ 16.2.0, CMake 4.4.4, Ninja 1.13.2, libcurl 8.23.0): CMake configure and
build with zero warnings, `ctest` green (84 tests), the menu smoke test, a cold download of all four symbols, the
walk-forward report, and a live signal through libcurl whose close matched an independent `curl.exe` request for the
same candle. The build copies the needed DLLs next to the executables, so they also run from a normal shell without the
MSYS2 `bin` folder on PATH.
