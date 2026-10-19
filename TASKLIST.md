# TASKLIST.md: Work queue and progress log

Work top to bottom. Tick a box only after you have run the check and seen it pass (see CLAUDE.md, Verification rules). Gate mode is set in CLAUDE.md (`STAGE_GATES`). Each task lists its acceptance check. SPEC.md has the details.

## Stage 0: Environment and scaffolding

- [x] **T0.1 Detect toolchain.** Run `g++ --version`, `cmake --version`, and find out whether libcurl headers and libs are available (for example `pkg-config --modversion libcurl`, or a CMake `find_package(CURL)` probe). Detect Ninja vs mingw32-make. Record exact versions and the chosen generator in DECISIONS.md. *Accept:* g++ supports C++17 and all of the above are found. If anything is missing, write a NEEDS_HUMAN item with the exact install command (for MSYS2 UCRT64: `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-ninja`).
- [x] **T0.2 Vendor nlohmann/json.** Download `https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp` to `third_party/nlohmann/json.hpp`. *Accept:* file exists and a one-line program that parses `[1,"2"]` compiles and runs. If the download is impossible, write a minimal JSON array parser instead and log it.
- [x] **T0.3 CMake project.** `CMakeLists.txt` with C++17, `-Wall -Wextra -Wpedantic`, default `Release`, a `quant_core` static library, the `quant_engine` executable, a `quant_tests` executable registered with `ctest`, and libcurl linked. *Accept:* configure and build succeed from an empty `build/`.
- [x] **T0.4 Test harness.** `tests/minitest.hpp`: `TEST(name)`, `CHECK`, `CHECK_NEAR`, `CHECK_THROWS`, auto-registration, non-zero exit on failure. *Accept:* a deliberately failing test makes `ctest` fail, a passing one passes (then remove the failing one).

**Gate G0:** clean configure + build + `ctest` all green. Record in the progress log.

## Stage 1: Architecture

- [x] **T1.1** ARCHITECTURE.md written and approved by Sidh (class design, concept map, stage plan).

## Stage 2: Core types, data layer, indicators

- [x] **T2.1 Core.** `Errors`, `Candle`, `CandleSeries` (with `operator[]`, `at`, `slice`, iterators, `operator<<`), `alignSeries`, `RollingWindow<T>`. *Accept:* SPEC §12 tests 6 and the series basics pass.
- [x] **T2.2 HTTP.** `IHttpClient`, `CurlHttpClient` (RAII handle, timeouts, CA handling, HTTP status surfaced, 429 back-off, no certificate-verification bypass), `MockHttpClient`. *Accept:* compiles; mock used by later tests.
- [x] **T2.3 BinanceDataSource.** URL building, paging, parsing, closed-candle filtering, dedupe, base-URL fallback, `fetchLatest`. *Accept:* SPEC §12 tests 1, 2, 3 pass.
- [x] **T2.4 CSV.** `CsvDataSource` and `DataCache` (incremental update, validation, clear errors). *Accept:* SPEC §12 test 4 passes.
- [x] **T2.5 Indicators.** `Indicator` base, `SMA`, `EMA`, `RSI`, `ZScore`, `ATR`. *Accept:* SPEC §12 test 5 passes.
- [x] **T2.6 Fixtures.** Create `tests/fixtures/klines_sample.json` (synthetic data in the real 12-field layout, labelled synthetic in a sibling README or a comment in the test). *Accept:* used by tests above.
- [x] **T2.7 Real-data smoke test, independently verified.** Download 5 days of `BTCUSDT` 1h through your own code into the cache. Then check: ascending, no duplicates, contiguous (`openTime` step = 3600000), OHLC sanity (`high >= max(open,close)`, `low <= min(open,close)`). Then, **independently**, run a separate raw `curl -sS "https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1h&startTime=<ms>&limit=1"` for three different candles (first, middle, last) and compare open/close/volume with the CSV rows. *Accept:* all three match exactly. Paste the three comparisons into the progress log. If Binance is unreachable on every base URL, follow escalation rule 2.

**Gate G2:** build clean with no warnings; all tests green; T2.7 evidence recorded.

## Stage 3: Strategy interface, simple strategies, backtester

- [x] **T3.1 Strategy framework.** `Strategy` base, `ParamSet`, `TargetWeight`, `MarketView`, `StrategyFactory` (registry with `create` and `registered`), `ParameterGrid`. *Accept:* factory creates every registered name.
- [x] **T3.2 Strategies.** `BuyAndHold`, `MeanReversion`, `Momentum`, `Breakout` per SPEC §6. *Accept:* each runs on real cached data and produces sensible target weights.
- [x] **T3.3 Engine.** `CostModel`/`BpsCostModel`, `Portfolio` (sole owner of cash and positions), `Backtester` with the exact timing and sizing rules of SPEC §7.
- [x] **T3.4 Metrics and results.** `PerformanceMetrics`, `BacktestResult`, `operator<<` printing.
- [x] **T3.5 Tests.** SPEC §12 tests 7 to 11 (toy dataset, cost check, leakage for every registered strategy, execution timing, metrics). *Accept:* all pass, with the hand-calculated numbers in comments.
- [x] **T3.6 Sanity run on real data.** Backtest the four strategies on cached BTCUSDT 1h. Check results are plausible (no Sharpe above 3, buy-and-hold CAGR matches the price move). Record a short table in the progress log.

**Gate G3:** all tests green including the leakage test; sanity table recorded.

## Stage 4: Models, pairs, ML, walk-forward, recommender

- [x] **T4.1 Models.** `Model` base, `RollingOLS`, `KalmanFilter`, `LogisticRegression`. *Accept:* SPEC §12 test 12 passes.
- [x] **T4.2 Advanced strategies.** `PairsTradingStrategy` (with ADF diagnostic) and `MLSignalStrategy` per SPEC §6. Extend the leakage test to cover them. *Accept:* leakage test passes for both.
- [x] **T4.3 Walk-forward.** `WalkForwardOptimizer` per SPEC §8. *Accept:* SPEC §12 test 15 passes.
- [x] **T4.4 Recommender and report.** `StrategyRecommender`, `ReportWriter` (CSV + console), verdict logic, disclaimer line. *Accept:* report columns exactly as in SPEC §8.
- [x] **T4.5 Synthetic validation.** SPEC §12 tests 13 and 14 (cointegrated pair makes money, random walk yields no `RECOMMENDED`).
- [x] **T4.6 Real-data run.** Download 730 days of 1h data for all default symbols (cache). Run the recommender over all universes. Save the report. *Accept:* finishes in about 5 minutes or less in Release. Check the numbers for leakage smell (CLAUDE.md, Verification rules). Record the top three rows and the verdict counts in the progress log, including if nothing is `RECOMMENDED`.

**Gate G4:** all tests green; real-data report exists in `reports/`; no unexplained suspicious metrics.

## Stage 5: Live mode, CLI, config

- [x] **T5.1 Observers.** `ISignalListener`, `ConsolePrinter`, `FileLogger`.
- [x] **T5.2 LiveRunner.** Polling, new-closed-candle detection, retrain/seed per SPEC §9, Ctrl-C handling, back-off, `--once`.
- [x] **T5.3 Config and Logger.** `config.txt` loader (`ConfigError` on bad lines), `Logger`.
- [x] **T5.4 MenuApp and main.** Menu per SPEC §10 including EOF handling.
- [x] **T5.5 End-to-end smoke tests.** (a) `printf "2\n0\n" | build/quant_engine.exe` exits 0 (SPEC test 16). (b) `build/quant_engine.exe --live --once` fetches a real latest candle and prints a signal line (or the clear "no recommended strategy" message). Record real output.

**Gate G5:** tests green; both smoke tests recorded with real output.

## Stage 6: Polish and viva material

- [x] **T6.1 README.md.** What it is, how to build and run on Windows/MinGW, the menu, honest limits (no borrow cost, backtests overfit, crypto regime dependence, signals only). Short.
- [x] **T6.2 docs/VIVA_NOTES.md.** For every row of the ARCHITECTURE.md §4 concept map: the exact file and class where it is used and a two-sentence justification. Then 15 likely viva questions with answers that refer to *this* code (for example: why composition for strategies and indicators, why `Portfolio` owns all state, how leakage is prevented, what walk-forward does, what the Sharpe formula is, why the mock HTTP client exists, what RAII protects). Plain language, written so Sidh can explain it himself.
- [x] **T6.3 Final cleanup.** Zero warnings, remove dead code, make sure comments are brief and explain design choices. Add the "As built" section to ARCHITECTURE.md (what actually exists, what differs from the plan and why).
- [x] **T6.4 Fresh-start verification.** `rm -rf build` (inside the project only), configure, build, `ctest`, run the menu smoke test, run option 4 from a cold cache download, run `--live --once`. Record real outputs.

**Gate G6 (done):** everything above verified in this session. Final summary (at most 10 lines) written at the end of the progress log.

---

## Progress log

Append newest at the bottom. Format: `YYYY-MM-DD HH:MM  Tx.y  what was run  ->  real result (short excerpt)`.

2026-10-06 20:30  T0.1  g++ --version; cmake --version; pkg-config; ls curl headers  ->  g++ 15.2.0 OK, make 4.4.1 OK, cmake/ninja/pkg-config/libcurl headers MISSING. Escalated as H-001 (NEEDS_HUMAN.md). T0.1, T0.3, T2.2 stay open.
2026-10-06 20:32  T0.2  curl -sSL json.hpp (v3.11.3) + 1-line parse of [1,"2"] with g++  ->  `2 2` (size 2, second element "2"). Done.
2026-10-06 20:34  T0.4  mingw32-make -f Makefile.dev tests with a deliberately failing test  ->  `2 tests run, 1 failed`, runner exit code 1; failing test removed. ctest registration still unverified (no cmake), so T0.4 stays open.
2026-10-06 20:38  T2.1/T2.3/T2.4/T2.5/T2.6  mingw32-make -f Makefile.dev tests (g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic, no warnings)  ->  `26 tests run, 0 failed` (core, RollingWindow 100k, parsing, paging 1000+37, unclosed candle, 429/418/451 handling, CSV round trip/corrupt/gap, indicators vs naive). Tests are built via the temporary Makefile.dev (D-015).
2026-10-06 20:40  T2.7  build_dev/fetch_dev.exe BTCUSDT 1h 5 (own BinanceDataSource + DataCache; transport = curl.exe CLI bridge, D-016)  ->  `BTCUSDT 1h 119 bars [2026-10-01 16:00Z .. 2026-10-06 14:00Z]`. Independent python check: ascending True, unique True, contiguous True, OHLC sane True. Raw `curl .../klines?symbol=BTCUSDT&interval=1h&startTime=<ms>&limit=1` vs CSV: first t=1790870400000 open 84162.66 close 84202.01 vol 751.73824 MATCH; middle t=1791082800000 open 84802.17 close 84840.87 vol 226.78471 MATCH; last t=1791295200000 open 86324.01 close 86595.61 vol 1008.47529 MATCH. Caveat: this exercised BinanceDataSource, not libcurl; re-run with CurlHttpClient in T6.4.
2026-10-06 21:00  T3.1-T3.5  mingw32-make -f Makefile.dev tests (-Wall -Wextra -Wpedantic, no warnings)  ->  `45 tests run, 0 failed`. Includes: toy dataset equity 10000/11000/12100/11000, maxDD 0.0909090909, totalReturn 0.10 (tol 1e-9); fee 10 bps equity 10989.010989; execution timing (signal at close of bar 2 fills at open of bar 3); metrics vs hand values; leakage test for all registered strategies on 4 param sets each, plus a deliberately cheating strategy that the same method DOES flag (test has teeth).
2026-10-06 21:02  data  build_dev/fetch_dev.exe {BTC,ETH,BNB,SOL}USDT 1h 730 (curl-CLI transport, D-016)  ->  `ETHUSDT 1h 17519 bars [2024-10-06 16:00Z .. 2026-10-06 14:00Z]`, same for BNB, SOL, and BTC after a DataCache fix (a short cache is now rebuilt when more history is requested; log: "BTCUSDT: cached history is shorter than requested, re-downloading"). 45 s total.
2026-10-06 21:05  T3.6  build_dev/sanity_dev.exe BTCUSDT (defaults, 10 bps fee + 2 bps slippage, whole series, no tuning)  ->
  BuyAndHold    return +38.161%  CAGR 17.543%  Sharpe 0.585  maxDD 53.7%  trades 1    (raw price move +38.07%, CAGR 17.50%: matches)
  MeanReversion return -37.814%  CAGR -21.143% Sharpe -0.749 maxDD 48.9%  trades 150  win 62.7%
  Momentum      return -34.048%  CAGR -18.790% Sharpe -0.532 maxDD 64.5%  trades 240  win 30.4%
  Breakout      return  -5.289%  CAGR  -2.681% Sharpe  0.018 maxDD 43.2%  trades 125  win 32.0%
  No Sharpe above 3; nothing suspiciously smooth. Plausible given ~12 bps round-trip costs and 100-475x turnover.
2026-10-06 21:06  GATE G3  tests green incl. leakage; sanity table above  ->  PASSED (verified through Makefile.dev; CMake/ctest verification still pending H-001).
2026-10-06 21:40  T4.1-T4.5  mingw32-make -f Makefile.dev tests (no warnings)  ->  `63 tests run, 0 failed` (5.6 s). Covers RollingOLS (beta 1.5, alpha 2 recovered; follows a regime change), Kalman (beta 1.2 recovered to 0.1; tracks drift), LogisticRegression (accuracy > 0.95 train and held-out), Dickey-Fuller, ML features, pairs strategy, WF window boundaries (no overlap, train < test), verdict rules, CSV columns/round trip, honest "no strategy beat buy-and-hold" text. Leakage test now also covers PairsTrading and MLSignal.
2026-10-06 21:41  T4.5  pairs on synthetic cointegrated pair (seed 42, train 1000/test 250)  ->  12 windows, OOS return +297%, Sharpe 12.4, 90 trades. Very high because the synthetic spread is built to be easy (3% OU spread vs 0.12% round trip); leakage test passes for PairsTrading, so this is not a real-data claim.
2026-10-06 21:42  T4.5  honesty test: two random walks (seed 42/43, 7000 bars, default WF)  ->  0 RECOMMENDED (best row PairsTrading WEAK, OOS Sharpe 1.17 vs baseline 1.14). Extra check, informational only, thresholds untouched: 20 other seeds x 9 rows = 180 rows -> 10 RECOMMENDED (5.6%), 8/20 seeds with at least one. Mean OOS Sharpe on noise is negative for every strategy (Breakout -0.44, MLSignal -2.66, MeanReversion -0.30, Momentum -0.05, Pairs -0.37), so no sign of leakage; the false positives are the multiple-testing effect SPEC section 8 warns about. The seed-42 test is deterministic and passes, but it is not a guarantee for other seeds (D-026).
2026-10-06 21:45  T4.6  build_dev/recommend_dev.exe (730 days 1h, 4 symbols, 3 pairs, 19 universe/strategy rows, 31 WF windows each, train 2000/test 500/step 500, 10 bps fee + 2 bps slippage)  ->  27.4 s. Verdicts: RECOMMENDED 0, WEAK 0, REJECT 19, INSUFFICIENT 0. Top three by score: #1 ETHUSDT Breakout score -0.91 OOS Sharpe -0.12 (B&H 0.08); #2 SOLUSDT Breakout -1.25, Sharpe -0.29 (B&H 0.00); #3 BTCUSDT Breakout -1.45, Sharpe -0.66 (B&H 0.03). Report line: "no strategy beat the buy-and-hold baseline out of sample on this data." Pair Dickey-Fuller t: ETH/BTC -1.95, SOL/ETH -1.52, BNB/ETH -2.30 (all above approx -3.34: cointegration not supported). No OOS Sharpe above 3, no smooth rising curve: no leakage smell. Report saved to reports/recommendation_20261006_152824.csv, identical to reports/latest_recommendation.csv (cmp).
2026-10-06 21:46  GATE G4  tests green; real-data report in reports/; metrics explained  ->  PASSED (via Makefile.dev; CMake/ctest still pending H-001).
2026-10-06 22:10  T5.1-T5.4  mingw32-make -f Makefile.dev tests (no warnings)  ->  `81 tests run, 0 failed`. New: config parsing + ConfigError with file/line, console line format, FileLogger header/rows, LiveRunner (seed, in-order polling, `changed` flag LONG->FLAT, catch-up after 4 missed candles, NetworkError back-off 1 s/2 s without crashing, --once, pair universe, not-enough-history error), menu (smoke "2,0" exit 0, EOF, invalid input, one-line errors, download/summary/backtest through MockHttpClient). Ctrl-C is covered by the stop-flag unit test; a manual Ctrl-C in a terminal was not exercised.
2026-10-06 22:12  T5.5a  `printf "2\n0\n" | build_dev/main_dev.exe`  ->  exit=0, `BTCUSDT    17519 bars  2024-10-06 16:00Z .. 2026-10-06 14:00Z  last close 86595.6  gaps 0`.
2026-10-06 22:13  T5.5b  `build_dev/main_dev.exe --live --once`  ->  exit=0, "No strategy has a RECOMMENDED verdict: none beat buy-and-hold out of sample on the last run." plus best-scoring row, not recommended. With `--shadow`: `2026-10-06 14:00Z ETHUSDT close=2722.2400 strategy=Breakout target=FLAT [NOT RECOMMENDED: shadow mode]`, exit=0, one row appended to logs/live_signals.csv. Independent raw curl of that ETHUSDT 14:00Z candle: close 2722.24000000 (match). Caveat: both smoke runs use the dev binary with the curl-CLI transport (D-016), not the libcurl `quant_engine.exe`; re-run in T6.4 once H-001 is resolved.
2026-10-06 22:14  GATE G5  tests green; both smoke tests recorded  ->  PASSED via Makefile.dev/main_dev (CMake build of src/main.cpp + CurlHttpClient still pending H-001).
2026-10-06 22:30  T6.1/T6.2  wrote README.md and docs/VIVA_NOTES.md (11 concept-map rows with file/class, 17 Q&As referring to this code). Claims in them were checked against the code (grid sizes, 2200-bar seed, 12 bps/side, 5.6% noise false-positive rate, 82 tests).
2026-10-06 22:35  T6.3  `mingw32-make -f Makefile.dev clean`, then full -j4 rebuild of tests + 4 dev tools with -Wall -Wextra -Wpedantic -O2  ->  0 warnings, `82 tests run, 0 failed` (18.8 s). `g++ -fsyntax-only` of src/data/CurlHttpClient.cpp and src/main.cpp against a stub curl.h -> no warnings (not a real libcurl build). Dead code: removed unused `CandleSeries::bars()`; added a test for `MarketView::history`; `<numeric>` now really used (accumulate). "As built" section added to ARCHITECTURE.md (section 9).
2026-10-06 22:40  T6.4 (partial, dev build)  rm data/*.csv reports/*.csv; `printf "1\n2\n4\n0\n" | build_dev/main_dev.exe`  ->  exit=0 in 85.8 s: 4 of 4 symbols downloaded cold (17519 bars each, 2024-10-06 16:00Z .. 2026-10-06 14:00Z, gaps 0), summary printed, walk-forward ran, "RESULT: no strategy beat the buy-and-hold baseline out of sample on this data.", saved reports/recommendation_20261006_153830.csv + latest_recommendation.csv (same 3909 bytes as the first run: deterministic). stderr empty. T6.4 stays OPEN: it must be repeated with the CMake build and the libcurl `quant_engine.exe` once H-001 is resolved.

### Status at the end of this session (summary)
Done and verified (via Makefile.dev): Stages 2, 3, 4, 5 gates and Stage 6 docs; 82 tests green, 0 warnings, real-data report generated, live signal fetched and cross-checked with raw curl.
Not done, blocked on NEEDS_HUMAN H-001 (cmake, ninja, libcurl not installed; installing system packages is outside my permission): T0.1 acceptance, T0.3 CMake project (CMakeLists.txt written, never configured), T0.4 ctest registration, T2.2 CurlHttpClient real build, T6.4 fresh-start verification, and therefore gates G0, G2, G6.
Next after Sidh runs the pacman command in H-001: re-run /build-next. It will configure/build with CMake, run ctest, verify CurlHttpClient and src/main.cpp against real libcurl (incl. CA bundle on Windows), re-run the T2.7 and T5.5 checks with libcurl, delete Makefile.dev and tools/, and tick the remaining boxes.

2026-10-18 10:30  session 2 (working tree moved to D:\D\College\...)  re-checked toolchain  ->  `which cmake ninja pkg-config` all missing; `pacman -Q` has no mingw-w64-ucrt-x86_64 cmake/ninja/curl/pkgconf (pacman.log last written 2026-02-21); no curl.h/cmake.exe/ninja.exe anywhere on C: or D:. Requested checks (1) CMake build + ctest, (2) live fetch through real CurlHttpClient, (5) gates G0/G2/G3-G5 under CMake are therefore NOT run and NOT claimed. H-001 updated with the evidence.
2026-10-18 10:45  (3) regression test  added `datacache_rebuilds_cache_that_does_not_cover_requested_history` (tests/test_data.cpp): 5-hour cache, 30 days requested -> request startTime == now-30d (not last bar+1h), series rebuilt to the 40 downloaded bars, file re-saved. Mutation check: with the rebuild condition disabled it FAILS (5 CHECKs, e.g. `CHECK(out.size() == 40)`); restored -> `83 tests run, 0 failed` via Makefile.dev, no warnings. (Compiler hit a transient "cc1plus.exe: out of memory" twice on this machine; make resumed and succeeded.)
2026-10-18 10:46  (4) DECISIONS.md  D-024 (cache rebuild when history is shorter than requested) and D-025 (Kalman z standardised by its rolling mean/std) are both recorded (lines 35-36). Verdict thresholds untouched (Defaults.hpp unchanged).

2026-10-18 18:05  T0.1  `cmake --version; ninja --version; pkg-config --modversion libcurl; g++ --version`  ->  cmake 4.4.4, ninja 1.13.2, libcurl 8.23.0, g++ 16.2.0 (all MSYS2 UCRT64; pacman.log shows the install at 15:38). Recorded in D-030. H-001 resolved.
2026-10-18 18:08  T0.3  `rm -rf build; cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release; cmake --build build -j`  ->  configure exit 0 (`Found CURL ... version "8.23.0"`), build exit 0, 0 warnings, FLAGS `-O2 -DNDEBUG -std=c++17 -Wall -Wextra -Wpedantic` (pinned -O2; CMake default was -O3). Fixed: exes failed from a bare shell with 0xC0000135 (DLL not found) -> POST_BUILD DLL copy (20 DLLs); a CMake CMP0207 policy warning from that script was fixed too.
2026-10-18 18:10  T0.4  ctest with a deliberately failing TEST added  ->  `0% tests passed, 1 tests failed out of 1`, ctest exit 8; removed -> exit 0.
2026-10-18 18:11  T2.2  CurlHttpClient built and linked against real libcurl. Live run from bare PowerShell (PATH = System32 only, no CA env vars): `build\quant_engine.exe --live --once --shadow` -> `2026-10-18 11:00Z ETHUSDT close=2418.2300 strategy=Breakout target=FLAT [NOT RECOMMENDED: shadow mode]`, exit 0. Independent `C:\Windows\System32\curl.exe .../klines?symbol=ETHUSDT&interval=1h&startTime=1792321200000&limit=1` -> open 2419.15 close 2418.23000000: MATCH.
2026-10-18 18:12  T6.4  rm data/*.csv reports/*.csv; bare PowerShell: `"1`n2`n4`n0" | build\quant_engine.exe`  ->  exit 0 in 57 s; 4 of 4 symbols, 17519 bars each [2024-10-18 13:00Z .. 2026-10-18 11:00Z], gaps 0; 19 rows, all REJECT (top: SOLUSDT Breakout score -1.15 OOS Sharpe -0.22; BTCUSDT Breakout -1.37/-0.66; ETHUSDT Breakout -1.47/-0.45); "RESULT: no strategy beat the buy-and-hold baseline out of sample on this data."; saved reports/recommendation_20261018_124205.csv + latest_recommendation.csv. Max OOS Sharpe -0.22: no leakage smell. DF t: ETH/BTC -1.97, SOL/ETH -1.48, BNB/ETH -2.33.
2026-10-18 18:13  T2.7 re-check through libcurl  BTCUSDT cache from the run above vs Windows curl.exe  ->  17519 rows, ascending/unique/contiguous/OHLC sane all True; first t=1729256400000 o/c/v 67662.00/68196.00/1233.37326 MATCH; middle t=1760788800000 107208.61/107065.17/265.11201 MATCH; last t=1792321200000 82093.76/82069.72/233.83733 MATCH.
2026-10-18 18:14  T3.6 re-check with build/quant_engine.exe option 3 (BTCUSDT, defaults)  ->  BuyAndHold +20.20% CAGR 9.636% Sharpe 0.429; MeanReversion -38.04% Sharpe -0.751 (152 trades); Momentum -40.88% Sharpe -0.719 (241); Breakout -11.07% Sharpe -0.113 (124). Raw price move +21.29%: the 1.1 pt difference is entry at bar 1 open (bar 0 rose 0.79%) plus 12 bps entry cost (1.2129/1.0079/1.0012 = 1.2019). No Sharpe > 3.
2026-10-18 18:15  T5.5 under CMake  `printf "2\n0\n" | build/quant_engine.exe` -> exit 0 with the 4-symbol summary; `build/quant_engine.exe --live --once` -> exit 0 with "No strategy has a RECOMMENDED verdict ..." and best row SOLUSDT Breakout NOT recommended; with `--shadow` -> `2026-10-18 11:00Z SOLUSDT close=104.0000 strategy=Breakout target=FLAT [NOT RECOMMENDED: shadow mode]`, exit 0, row appended to logs/live_signals.csv.
2026-10-18 18:20  extra tests  `datacache_rebuilds_cache_that_does_not_cover_requested_history` (regression for D-024; fails 5 CHECKs with the fix disabled) and `logger_writes_levels_to_file` (engine.log path had no test).
2026-10-18 18:25  cleanup  deleted Makefile.dev, tools/, build_dev/, logs/_cli_*.tmp (D-030); `rm -rf build`, configure, build -> 0 warnings; `ctest --test-dir build --output-on-failure` -> `100% tests passed out of 1`; `build/quant_tests.exe` -> `84 tests run, 0 failed`; filters: no_lookahead_for_every_registered_strategy, leakage_check_has_teeth..., honesty_random_walk_is_never_recommended each `1 tests run, 0 failed`.

GATES re-checked under CMake (not Makefile.dev), 2026-10-18:
  G0 PASSED: clean configure + build, 0 warnings, ctest green.
  G2 PASSED: 0 warnings, all tests green, T2.7 evidence re-done through libcurl (3/3 match).
  G3 PASSED: tests green incl. leakage test (and its teeth test); sanity table above.
  G4 PASSED: tests green; reports/recommendation_20261018_124205.csv from real data; no suspicious metrics (best OOS Sharpe -0.22).
  G5 PASSED: tests green; both smoke tests recorded above with the libcurl binary.
  G6 PASSED: everything above verified in this session; verdict thresholds unchanged (Defaults.hpp 0.2 / 0.5 / 1.0).

### Final summary (2026-10-18)
Done: all tasks T0.1-T6.4 ticked; gates G0, G2-G6 passed under CMake + libcurl.
Verified: 84 tests via ctest, 0 warnings at -O2; cold download + walk-forward report + live signal with the real exe from a bare shell; live close and 3 cached candles match independent curl.exe requests.
Result: no strategy beat buy-and-hold out of sample on 2 years of hourly data (19/19 REJECT); reported plainly.
Blocked: nothing. Open human items: none.


### T6.5 launcher (2026-10-19, small scoped task, not a new stage)
2026-10-19 10:00  T6.5  created run_quant.bat (cd /d "%~dp0"; build-first check with exit /b 1; runs build\quant_engine.exe %*; echo.; pause; exits with the exe's code) and .gitattributes (`*.bat text eol=crlf`); D-032 in DECISIONS.md; "Quick start (double-click)" in README.md. VIVA_NOTES skipped (no fitting section). No C++, tests, CMake or threshold changes.
  1) `file run_quant.bat`  ->  `run_quant.bat: DOS batch file, ASCII text, with CRLF line terminators`
  2) plain cmd.exe, PATH = System32 only, started from C:\ (`cwd=C:\`), `(echo 2& echo 0)| "D:\D\College\Sem 3\OOP\Course Project\quant_engine\run_quant.bat"`  ->  menu printed; `Data summary (data, interval 1h):` then `BTCUSDT    17519 bars  2024-10-18 13:00Z .. 2026-10-18 11:00Z  last close 82069.7  gaps 0` (and ETH, BNB, SOL); `Press any key to continue . . .`; `launcher_exit=0`. No DLL error, data\ found from a different working directory.
  3) renamed build\quant_engine.exe -> .hidden, ran launcher  ->  `build\quant_engine.exe was not found. Build the project first: see README.md, "Build and run".` + pause, `launcher_exit=1`; renamed back: `exe restored: yes` (build\quant_engine.exe present, 774610 bytes, dated Oct 18 18:41).
  extra) argument pass-through `run_quant.bat --live --once --shadow`  ->  `2026-10-18 22:00Z SOLUSDT close=103.9000 strategy=Breakout target=FLAT [NOT RECOMMENDED: shadow mode]`, `launcher_exit=0`.
  4) `ctest --test-dir build --output-on-failure`  ->  `100% tests passed out of 1`; `build/quant_tests.exe | tail -5`  ->  `84 tests run, 0 failed`.
  No .git folder exists, so nothing was committed (CLAUDE.md: do not git init).
