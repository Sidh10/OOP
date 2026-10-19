# NEEDS_HUMAN.md

The only place Claude Code asks Sidh for anything. Read CLAUDE.md for when asking is allowed.

**How it works:** Claude Code adds an item under "Open items" and keeps working on anything not blocked. Sidh answers by writing under the item's `Answer:` line, then runs `/build-next` (or just starts Claude Code again). Claude Code reads the answer, acts on it, and moves the item to "Resolved".

## Open items

_(none)_

## Resolved

### H-001  Install cmake, ninja, libcurl (toolchain incomplete)
Date: 2026-10-06
Blocked task: T0.1 (acceptance), T0.3, T2.2 CurlHttpClient compile, T2.7 real download via own code, T5.2/T5.5b live mode
What I tried: `g++ --version` -> 15.2.0 (MSYS2; both /mingw64 and /ucrt64 have it; C++17 compile with -Wall -Wextra -Wpedantic OK). `mingw32-make` 4.4.1 found. `cmake`, `ninja`, `pkg-config` not found anywhere under C:\msys64 or Program Files. No `curl/curl.h` and no libcurl import lib anywhere (only the Git-for-Windows curl.exe CLI, which has no headers). Binance is reachable: `curl https://api.binance.com/api/v3/ping` -> HTTP 200. Installing system packages is outside what I may do on my own (CLAUDE.md escalation rules 1 and 7).
Question: Please run this in the MSYS2 UCRT64 shell, then re-run /build-next:
```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf
```
I will call tools by absolute path under `C:\msys64\ucrt64\bin` either way, so no PATH change is needed from you.
Default if no reply: I keep building everything that does not need libcurl/cmake, compiling with g++ / mingw32-make through a temporary `Makefile.dev`, and keep `CurlHttpClient` written but unverified. T0.3, the CurlHttpClient compile, T2.7, T5.2, T5.5b and the real-data stages (T3.6, T4.6) stay blocked until the packages exist.
Answer:
Status 2026-10-18 (Claude Code): Sidh said the packages are installed, but they are NOT visible on this machine. `pacman -Q` lists no mingw-w64-ucrt-x86_64-{cmake,ninja,curl,pkgconf}; `/c/msys64/var/log/pacman.log` last changed on 2026-02-21; no cmake.exe, ninja.exe or curl/curl.h exists anywhere under C:\msys64, C:\Program Files or D:\. Only the MSYS-runtime `curl 8.17.0` / `libcurl 8.17.0` packages (not the mingw ones) are installed. Please run the pacman command above in the **MSYS2 UCRT64** shell of THIS machine (C:\msys64) and tell me, or check that the install went to a different MSYS2 root and give me its path. Until then CMake/ctest/libcurl checks cannot be run and I will not claim them.
Resolved 2026-10-18: packages present (cmake 4.4.4, ninja 1.13.2, libcurl 8.23.0, pkgconf); CMake build, ctest and the libcurl live fetch verified. See TASKLIST progress log.

## Template for a new item

```
### H-001  <short title>
Date: YYYY-MM-DD
Blocked task: Tx.y (or "none, informational")
What I tried: <commands run and what came back, briefly>
Question: <one precise question, or the exact thing Sidh must do>
Default if no reply: <what Claude Code will assume or skip>
Answer:
```
