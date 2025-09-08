# Repository Guidelines

This guide aligns contributors on how to build, test, and submit changes for the Sol Reader glasses firmware.

## Project Structure & Module Organization
- `src/`: C/C++ app code (entry: `src/main.cpp`). Key areas: `Communications/`, `Displays/`, `EPub/`, `Inputs/`, `Models/`, `Renderers/`, `Sensors/`, `Storage/`, `UI/`, `Ulp/`.
- `include/`: generated or shared headers. `components/` and `lib/`: external components and Simulator code.
- `test/CatchTests/`: Catch2 tests; images under `Images/Accepted/` (baselines) and `Images/TestRun/` (diffs).
- `scripts/`: helper tools; `docs/`, `data/`: assets and docs. Build output goes to `build*/`.

## Build, Test, and Development Commands
- Build firmware: `. ~/esp/esp-idf/export.sh && make build`
- Flash + monitor: `make flash monitor` (set serial ports automatically). Stop reset-only monitor with `make monitor`.
- Target profiles: `TARGET=glasses-release make build` (see `sdkconfig.defaults.*`).
- Simulator: `make sim` (or `sim8`, `sim16`, `sim24`); logs in `logs/`.
- Tests: `make test` (or focused: `make test-kindle`, `make test-content-sync`).
- Lint/format: `make clang-format` or `make tidy`; clean: `make clean`.

## Coding Style & Naming Conventions
- C++ style: LLVM base, 4-space indent, 100-col limit (`.clang-format`).
- Includes: group/regroup system `<...>` before local "...".
- Naming (`.clang-tidy`): namespaces `lower_case`; classes/structs `CamelCase`; functions `camelBack`; variables `camelBack`; constants `UPPER_CASE`; private/protected members end with `_`.
- Run `make tidy` before pushing. Python files are auto-formatted in CI via `black` and `isort`.

## Testing Guidelines
- Framework: Catch2 in `test/CatchTests`. Add files as `Test*.cpp` or `<Feature>Test.cpp`.
- Tags: use category tags (e.g., `[epub]`, `[content-sync]`, `[telemetry]`, `[kindle]`).
- Visual baselines: when a baseline is missing or mismatches, tests write new images to `.../TestRun/` and also copy them into the matching `.../Accepted/` location—but the run still fails (by design) so CI flags it.
- Local acceptance: run `make test`, review diffs, then leave the updated baselines in Accepted/ unstaged to accept them, then rerun.

## Commit & Pull Request Guidelines
- Messages: imperative, concise; reference issues/PRs with `#1234`. Example: `Fix: prevent green flash (#1143)`.
- PRs: include a summary, linked issues, test plan (commands run), and screenshots or updated baselines for UI changes.
- CI must pass: Catch2 tests, clang-tidy/format, and Python formatting workflows run on PRs. Run `make test` and `make tidy` locally first.

## Security & Configuration Tips
- Do not commit secrets or local prefs (e.g., `simprefs.ini`, logs, `TestRun/` outputs are gitignored).
- Set up ESP-IDF per README; export the environment before building. For Simulator cloud access, keep tokens in `simprefs.ini` locally.
