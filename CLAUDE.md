# Sol Reader Embedded App - Development Guide

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

### Navigation Structure
The main menu has five sections that you navigate between using LEFT/RIGHT, in this order:
1. **Recent** - Recent EPUBs
2. **Books** - Local EPUB and PDF books
3. **Kindle** - Kindle books (require network connection)
4. **Articles** - Pocket articles and daily summaries
5. **Settings** - Preferences and system
Most of the time, you'll be on the Recent screen when you start the simulator.

### Reading Screenshots
Always take a screenshot with `--key p` and read it using the Read tool on `./screenshot.png` to understand the current UI state. The simulator displays:
- Header showing current section with navigation arrows (< Books >)
- List of items in the current section
- Selected item is highlighted with a white background
- Time shown at bottom (e.g., "6:45 pm")

### Opening Books
- **Regular Books (EPUB/PDF)**: Press ENTER on a book in the Books section - opens immediately
- **Kindle Books**: Press ENTER on a book in the Kindle section
  - Shows "Opening your Kindle book..." 
  - After ~30 seconds shows "This can take up to 60 seconds"
  - **Can take up to 60 seconds to fully load** while it connects to the network and fetches content
  - Once loaded, shows the book text

### In-Book Navigation
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

### Popup Menu Navigation
- **UP/DOWN**: Navigate between menu items
- **ENTER**: Select current item
- **ESCAPE**: Close popup (Back to Text)

### Best Practices for Automation
1. Always kill existing simulators before starting a new one
2. Wait 3-4 seconds after starting for initialization
3. Take screenshots frequently to verify UI state
4. Add delays (1-2 seconds) after navigation commands
5. For Kindle books, wait up to 60 seconds for content to load
6. Read screenshots using the Read tool to interpret the UI
7. The socket file `./sol_simulator_automation.sock` is created in the current directory

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
