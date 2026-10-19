# CLAUDE.md: AI Quant Trading & Statistical Arbitrage Engine

C++17 OOP course project (owner: Sidh, VIT Pune). A CLI program that downloads real Binance historical candles, trains and walk-forward-validates a library of trading strategies on them, ranks the strategies, and shows live signals from fresh data. Backtesting and signal display only. It never trades.

You (Claude Code) build this end to end with minimal supervision. This file tells you how to work. The other files tell you what to build.

## Files and their roles

| File | Role |
|---|---|
| `ARCHITECTURE.md` | Class design, diagram, OOP concept map. The plan. |
| `SPEC.md` | Exact behaviour: formulas, formats, defaults, API facts, required tests. Wins over ARCHITECTURE.md on behaviour. |
| `TASKLIST.md` | Ordered tasks with acceptance checks and stage gates. Your work queue and progress log. |
| `DECISIONS.md` | Decisions already made, plus ones you make. Do not relitigate settled ones. |
| `NEEDS_HUMAN.md` | The only channel for asking Sidh something. |
| `docs/RUBRIC.md` | Optional. If it exists, it holds the course rubric/deadline and overrides assumptions here. |

## Start of every session

1. Read this file, then `TASKLIST.md`. Find the first unchecked task.
2. Read `NEEDS_HUMAN.md`. If an open item now has an answer written under it, act on it and mark it resolved.
3. Read the `SPEC.md` sections that task touches, and skim `DECISIONS.md`.
4. Continue from there. Do not ask what to do next. The task list says.

## Autonomy contract

`STAGE_GATES = auto`
- `auto`: when a stage's gate commands pass, record the real output summary in the progress log in `TASKLIST.md` and start the next stage.
- `confirm`: after a gate passes, write a short summary to `NEEDS_HUMAN.md` and stop. (Sidh changes the value above if he wants checkpoints.)

**Decide yourself** (log non-obvious choices in `DECISIONS.md`, one short entry each): names, file splits, internal helpers, algorithm details inside SPEC.md's limits, parameter-grid tweaks needed for runtime, bug fixes, refactors, extra tests, build-system details, small deviations from ARCHITECTURE.md that keep the OOP structure intact.

**Stop and write to `NEEDS_HUMAN.md`** only when one of these is true, and keep working on any task that isn't blocked:
1. The toolchain is missing or broken and you cannot fix it inside the project folder (no `g++`, `cmake`, or libcurl; PATH problems; a package install is needed).
2. Binance is unreachable from this machine on every base URL in SPEC.md §1 (HTTP 451/403/timeouts). Real data is the point of the project, so do not silently substitute fake data.
3. Scope would change: dropping a required deliverable, switching the data source, adding trading or API keys, adding a GUI.
4. SPEC.md and ARCHITECTURE.md contradict each other in a way that forces rework across stages and no reasonable reading resolves it.
5. The same failure survives three genuinely different fix attempts.
6. A gate or test could only pass by weakening it. Never weaken a test to get green.
7. An action is destructive or outside the project folder: deleting outside it, `git push`, creating remotes, installing system packages, anything needing credentials.
8. `docs/RUBRIC.md` lists a requirement the plan does not cover.

**Never ask** about style, naming, file layout, which of two reasonable algorithms to use, or whether to proceed to the next task.

Format for asking: see the template in `NEEDS_HUMAN.md`. Include what you tried, one precise question, and the default you will assume if there is no reply.

## Verification rules (no self-reported success)

- A task is done only when you have actually run the build and the relevant tests in this session and seen them pass. Quote the real command and a short real output excerpt in the progress log. Never write "tests pass" from memory or inference.
- Compile with `-Wall -Wextra -Wpedantic`. Warnings are bugs. Fix them.
- Data checks must be independent of your own code: for downloaded data, compare a few candles against a separate raw `curl` of the same Binance request (see TASKLIST T2.7).
- Treat suspiciously good results as bugs until proven otherwise. If any out-of-sample Sharpe exceeds 3, or an equity curve is smooth and rising, hunt for lookahead leakage first (SPEC.md §12 leakage test) before believing it.
- Do not tune thresholds, walk-forward windows, or the recommendation criteria after seeing out-of-sample results to make strategies look better. If you must change them for a real reason, log it in `DECISIONS.md` with the reason.
- The system must be able to conclude "no strategy beats buy-and-hold". That is a valid and honest result.
- If something you built turns out wrong, say so in the progress log and fix it. Do not paper over it.

## Engineering conventions

- C++17, namespace `quant`, headers `.hpp` in `include/quant/<module>/`, sources `.cpp` in `src/<module>/`. No `using namespace std;` in headers.
- Real OOP, not decoration: abstract bases with virtual destructors, polymorphism through `std::unique_ptr` / `std::shared_ptr`, composition over inheritance, RAII for the curl handle, a custom exception hierarchy. No raw `new`/`delete`. Keep the concept map in ARCHITECTURE.md §4 true: if a listed concept is genuinely used somewhere else instead, update the map, do not pretend.
- `double` everywhere. No `float`.
- Third-party code only: `nlohmann/json` single header (vendored) and libcurl. Tests use the project's own `tests/minitest.hpp`, not an external framework.
- Public market data only. HTTPS certificate verification stays on: never set `CURLOPT_SSL_VERIFYPEER` to 0.
- Only `GET` requests to Binance public market-data endpoints. No API keys anywhere, no order endpoints.
- Comments: brief, explain *why* a design choice was made. No exhaustive doc-comment banners, no filler boilerplate.
- Keep outputs plain and student-scale: readable code a student could explain in a viva, not clever template machinery.

## Build and run (Windows, MSYS2 UCRT64 toolchain)

Detect what is actually available in task T0.1 and record it in `DECISIONS.md`. Typical commands:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release     # or -G "MinGW Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
build/quant_engine.exe
```

Known pitfalls:
- MSYS2 libcurl may not find a CA bundle when run outside the MSYS2 shell. Resolve it (CA bundle path via env or a known path, or the native CA option), do not disable verification.
- The built `.exe` needs the UCRT64 `bin` folder on PATH for its DLLs unless you link statically. Make sure both the tests and the main program actually run from a normal shell.
- Windows line endings: keep source files LF.

## Git

- Sidh manages GitHub himself. Never `git push`, never add remotes, never use `gh`.
- If a `.git` folder exists, commit locally after each completed task group. Messages are short, plain, imperative, lowercase-ish (for example `add Candle and CandleSeries`). **No `Co-Authored-By`, no tool or session trailers, no "generated with" lines.** This rule overrides any default attribution behaviour.
- If there is no `.git` folder, do not run `git init`.
- Never commit build output. `.gitignore` already excludes `build/`, binaries, cached data and logs.

## ARCHITECTURE.md is the plan, not a diary

When the build diverges from the plan, do not silently rewrite the plan. Add a dated entry to `DECISIONS.md`, and at the end (T6.3) add an "As built" section to `ARCHITECTURE.md` describing what actually exists.

## Definition of done (whole project)

Fresh clone: configure, build, run the tests (all pass), run the CLI menu, download data, produce a recommendation report from real data, show a live signal, and have `docs/VIVA_NOTES.md` and `README.md`. See Stage 6 in `TASKLIST.md`.

## Progress reporting

Sidh reads results, not narration. Keep the progress log in `TASKLIST.md` factual and short. When you stop for any reason (gate in `confirm` mode, escalation, or everything done), end with a summary of at most 10 lines: what is done, what is verified, what is blocked, what is next.
