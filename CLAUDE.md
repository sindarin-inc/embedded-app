# Sol Reader Embedded App - Development Guide

## Tips
- **Visual Test Generation**: When tests use `ASSERT_VIEW_UNCHANGED`, they automatically generate accepted images on first run if they don't exist. The test will fail initially and show you the copy command to create the accepted image from the generated TestRun image.

## Test Screenshots
- Never copy TestRun images to Accepted folder - the test runner auto-accepts them when tests fail
- The test framework automatically updates accepted screenshots when they change

## Build Commands
- Run simulator: `make sim` (automation socket is automatically created at `./sol_simulator_automation.sock`)
- Build for device: `. $IDF_PATH/export.sh && make build`
  - Always run these together in a single command
  - The export.sh sets up the ESP-IDF environment which is required for building
- Flash to device: `make flash`
- Monitor serial logs: `make monitor`
- Run simulator: `make sim` (or `make sim8`, `make sim16`, `make sim24` for different display modes)
  - No need to run export.sh for simulator
  - Simulator output is logged to `logs/sim.log` (or `logs/sim8.log`, `logs/sim16.log`, `logs/sim24.log`)
  - Watch logs in real-time: `tail -f logs/sim.log`
  - Log file is cleared on each run
- Run tests: `make test`
- Build tests: `make test-build`
- Run single test: `make test-build && ./test/CatchTests/build/tests '[test-tag]'` (e.g. `'[epub]'`, `'[content-sync]'`)
  - Always rebuild before running tests to ensure changes are included
- Format code: `make clang-tidy`
  - **IMPORTANT**: Always run `. $IDF_PATH/export.sh && make clang-tidy` after making code changes
- Full linting (format + static analysis): `make clang-format` (can be slow)

## Code Style Guidelines
- Format: Use clang-format with LLVM-based style (ColumnLimit: 100, IndentWidth: 4)
- Includes: Grouped with angle brackets (<>) first, then quotes
- Error handling: Use assertions for programming errors (e.g., `assert(notifyFlags_ != nullptr)`)
- Logging: Use log_d/log_i/log_w/log_e with descriptive messages
- Memory: Manage heap allocations carefully, use HEAP_PROFILE macros to track
- Naming: camelCase for variables/methods, PascalCase for classes, ALL_CAPS for constants/macros
- Comments: Use `//` for line comments and for block comments
- Stack usage: Monitor stack usage with watermarks (SHOW_WATERMARK macro)
- Threading: Use DispatchQueue for most async operations
- Comments: Only include comments if they add context that's not readily apparent in the next line of code or if the code block has some complexity
- Comments: Don't add comments that are simply addressing the prompt, only add them if the comments clear up confusion

## Architecture Conventions
- **Network/API requests**: All network requests must go through SindarinAPI (never direct HTTP calls from UI screens). All SindarinAPI methods should return `int` error codes and use output parameters for data (e.g., `int fetchData(Response *resp, ...)`) rather than returning data directly
- **Kindle navigation**: Kindle-specific logic and state management should be delegated to KindlePageFlipper, not handled in UI screens
- **Async operations**: Use DispatchQueue for background operations; UI screens coordinate but don't implement async logic directly
- **State management**: Business logic classes (like KindlePageFlipper) encapsulate state changes; UI screens handle callbacks and visual updates
- **Error handling**: Use assertions (`assert()`) for programming errors; return structured error types for recoverable operations
- **Data persistence**: UI screens use LocalStore and PreferencesStore for data access; avoid direct file system operations in UI
- **Telemetry**: Use centralized Telemetry class with structured fields; log meaningful events at appropriate levels (log_d/log_i/log_w/log_e)

## Simulator Automation

### Starting the Simulator with Automation
```bash
# Kill any existing simulator instances
pkill -f "simulator" || true

# Build and start simulator (automation is always enabled)
make sim-build
./lib/Simulator/build/simulator &
```

### Controlling the Simulator
Use the `control_simulator.py` script to send commands:

```bash
# Send individual keys
python3 scripts/control_simulator.py --key right   # Navigate right
python3 scripts/control_simulator.py --key left    # Navigate left
python3 scripts/control_simulator.py --key up      # Navigate up
python3 scripts/control_simulator.py --key down    # Navigate down
python3 scripts/control_simulator.py --key enter   # Select/confirm
python3 scripts/control_simulator.py --key escape  # Back/popup menu
python3 scripts/control_simulator.py --key p       # Take screenshot (saves to ./screenshot.png)

# Send key sequences
python3 scripts/control_simulator.py --sequence "rrll"  # right, right, left, left
```

### Reading Screenshots: 
Always take a screenshot with `--key p` and read it using the Read tool on `./screenshot.png` to understand the current UI state.

### Navigation Structure
The main menu has five sections that you navigate between using LEFT/RIGHT, in this order:
1. **Recent** - Recent EPUBs
2. **Books** - Local EPUB and PDF books
3. **Kindle** - Kindle books (require network connection)
4. **Articles** - Pocket articles and daily summaries
5. **Settings** - Preferences and system
Most of the time, you'll be on the Recent screen when you start the simulator.

### View Navigation

#### Opening Books
- **Regular Books (EPUB/PDF)**: Press ENTER on a book in the Books section - opens immediately
- **Kindle Books**: Press ENTER on a book in the Kindle section
  - Shows "Opening your Kindle book..." 
  - After ~30 seconds shows "This can take up to 60 seconds"
  - **Can take up to 60 seconds to fully load** while it connects to the network and fetches content
  - Once loaded, shows the book text

#### In-Book Navigation
When a book is open:
- **RIGHT**: Next page
- **LEFT**: Previous page
- **ESCAPE**: Opens popup menu with options:
  - Main Menu - Return to main menu
  - Contents - Show table of contents
  - Autopilot - Auto-scrolling mode
  - Archive - Archive the book
  - Brightness - Adjust screen brightness
  - Back to Text - Close popup and return to reading
  - Shows reading progress (e.g., "4 pages left", "38 of 852")

#### Popup Menu Navigation
- **UP/DOWN**: Navigate between menu items
- **ENTER**: Select current item
- **ESCAPE**: Close popup (Back to Text)