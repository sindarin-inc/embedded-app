# Sol Reader Embedded App - Development Guide

## Test Screenshots
- Never copy TestRun images to Accepted folder - the test runner auto-accepts them when tests fail
- The test framework automatically updates accepted screenshots when they change

## Build Commands
- Build for device: `. $IDF_PATH/export.sh && make build`
  - Always run these together in a single command
  - The export.sh sets up the ESP-IDF environment which is required for building
- Flash to device: `make flash`
- Monitor serial logs: `make monitor`
- Run simulator: `make sim` (or `make sim8`, `make sim16`, `make sim24` for different display modes)
  - No need to run export.sh for simulator
- Run tests: `make test`
- Run single test: `./test/CatchTests/build/tests '[test-tag]'` (e.g. `'[epub]'`, `'[content-sync]'`)
- Format code: `make clang-format`
- Static analysis: `make clang-tidy`

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
- Comments: Only include comments if they add context that's not readily apparent in the next line of code or if the code block has some complexity
- Comments: Don't add comments that are simply addressing the prompt, only add them if the comments clear up confusion
