# CHANGELOG

All notable project-wide changes will be documented in this file. Note that each subproject has its own CHANGELOG.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and [Commons Changelog](https://common-changelog.org). This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Types of Changes

- Added: for new features.
- Changed: for changes in existing functionality.
- Deprecated: for soon-to-be removed features.
- Removed: for now removed features.
- Fixed: for any bug fixes.
- Security: in case of vulnerabilities.

---

## [Unreleased]

### Added

- **`BogAudioCallbacks.note`** receives every event with its voice name, so any voice atom reaches the host. Previously the scheduler dropped atoms other than the eight built-in names. The named callbacks are used only when `note` is unset.

- **`bog_live_evaluator_set_transition`**: evaluate swaps on the transition manager's next bar line. The evaluator keeps the playing program's arena until the scheduler swaps. `bog_scheduler_get_program` exposes the playing program.

- **`BOG_MIN_STACK_SIZE`** (512 KB): the stack a query thread needs. A query at `BOG_MAX_DEPTH` takes up to 240 KB unoptimized and overflows a 128 KB thread. A test resolves runaway queries on a thread of this size.

- **`bog_scheduler_set_dispatch_on_time`** holds queried events until they are due, for hosts whose output cannot be timestamped. Off by default; `bog_midi` already timestamps.

### Fixed

- **`bog_midi` dropped voices other than the eight built-in names**: it set only the named callbacks. It now sets `note`. Drum voices keep their GM notes on channel 10; any other voice plays its pitch on the synth channel.

- **Recursion was broken, and a self-recursive clause crashed the process**: the resolver restarted clause renaming at 0 on every call, so each recursion level reused the variable names of the level above. `count(N) :- N > 0, M is N-1, count(M).` failed beyond one level. `event(V,P,X,T) :- event(V,P,X,T).` bound a variable to itself, and substitution then recursed until the stack overflowed. Renaming is now per query. Queries also stop at `BOG_MAX_DEPTH` (512), `BOG_MAX_STEPS` (20000) or `BOG_MAX_SOLUTIONS` (1024) and set `BogSolutions.truncated`.

- **Random builtins shared the C library stream**: `prob`, `pick`, `rand` and `randint` used `rand()`, which was never seeded, so every launch played the same sequence. Each `BogStateManager` now owns a clock-seeded stream; `bog_state_manager_seed` makes a run repeatable.

- **Each slot was queried on every tick**: `bog_scheduler_tick_at` re-queried the whole lookahead window each call. With `bog_midi`'s 5 ms tick and 80 ms lookahead, each note was sent 41 times. Ported from psnd.

- **Program swaps landed up to one lookahead after the bar line**: events past the bar were already queried from the old program. The scheduler now swaps when a query reaches the bar. If the bar has already been queried, the swap moves to the next bar.

- **A swap scheduled before `bog_scheduler_start` could wait on a stale clock**: hosts restart the clock in `start`, so the boundary could lie far ahead. `start` now applies a pending swap at once.

- **The build failed on Linux**: strict C11 hides `strdup`, `getline` and `ssize_t`, and the implicit `strdup` truncated pointers to `int`. C targets now define `_POSIX_C_SOURCE=200809L`.

## [0.1.0] - Initial Release

- Project created
