# Sol Reader Embedded App - Development Guide

## Build Commands
- Set up ESP-IDF environment: `. $IDF_PATH/export.sh`
- Build for device: `make build`
- Flash to device: `make flash`
- Monitor serial logs: `make monitor`
- Run simulator: `make sim` (or `make sim8`, `make sim16`, `make sim24` for different display modes)
- Run tests: `make test`
- Run single test: `./test/CatchTests/build/tests '[test-tag]'` (e.g. `'[epub]'`, `'[content-sync]'`)
- Format code: `make clang-format`
- Static analysis: `make clang-tidy`
- There's no need to run the idef export.sh script only if running `make sim`. It's only for `make build`
- You only need to run the export.sh idf script once per session. It's slow but necessary to `make build` but only the first time in the terminal

## Code Style Guidelines
- Format: Use clang-format with LLVM-based style (ColumnLimit: 100, IndentWidth: 4)
- Includes: Grouped with angle brackets (<>) first, then quotes
- Error handling: Use assertions for programming errors (e.g., `assert(notifyFlags_ != nullptr)`)
- Logging: Use log_d/log_i/log_w/log_e with descriptive messages
- Memory: Manage heap allocations carefully, use HEAP_PROFILE macros to track
- Naming: camelCase for variables/methods, PascalCase for classes, ALL_CAPS for constants/macros
- Comments: Use `//` for line comments, `/* */` for block comments
- Stack usage: Monitor stack usage with watermarks (SHOW_WATERMARK macro)
- Threading: Use DispatchQueue for most async operations