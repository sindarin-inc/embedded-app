# Sol Reader Glasses Firmware

All the embedded code that runs on the glasses themselves.

## Getting started (build with IDF.py)

After cloning this repo from GitHub you'll need to pull the most recent submodules. You'll need to rerun this command periodically to get the latest changes whenever the submodules change.

```bash
git submodule update --init --recursive
```

The primary way to build the firmware itself is with Espressif's official [esp-idf toolchain](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html). In some other directory (not in this repo), run:

```sh
git clone -b v5.1.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

If you get errors about `cmake` you might need to install `cmake` first (`brew install cmake` on macOS / `sudo apt install cmake` on Ubuntu).

Now from the root of this repo, you can run (replace `/YOUR/PATH/TO` with the path where you put `esp-idf`):

```sh
. /YOUR/PATH/TO/esp-idf/export.sh
make build flash monitor
```

This will build the latest firmware, flash it to the device, and open a serial monitor. You can also use `idf.py menuconfig` to configure the build if you need to change build options.

See also the official [esp-idf installation instructions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) for more details.

## Building alternate targets

By default we build a debug build for the glasses. But we support other build targets. To specific a different target, you can use the `TARGET` environment variable. For example, to build a release build, you can use:

```sh
TARGET=glasses-release build flash monitor
```

## `idf.py monitor` but without resetting

By defualt `idf.py monitor` will reboot the MCU as soon as it connects so that you can get logs from the very start of execution. If you want to connect to the MCU without rebooting it, you can use `idf.py monitor --no-reset -p [port]`. Here's a handy command to connect to the first device named `/dev/cu.usbmodem*`.

```sh
idf.py monitor --no-reset -p $(ls /dev/cu.usbmodem* 2> /dev/null | head -n 1)
```

## Install recommended tools

* Visual Studio Code (`brew install visual-studio-code`)
* CMake (`brew install cmake`, or on Ubuntu `sudo apt install cmake`)
* clang-format (`brew install clang-format`)

## Reformat code with `clang-format`

Make sure you have `clang-format` installed (`brew install clang-format`). Then if you're using VSCode, and install the recommended workspace extensions, your code should be formatted automatically for you on save (or with shift-option-f). Alternately you can force formatting of all code with:

```bash
find src -iname "*.hpp" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.c" | xargs clang-format -i
```

## Static analysis and cleanup with `clang-tidy`

You can run clang-tidy to do some more advance C++ static analysis and fixes. Make sure you've set up IDF, then run the following to automatically fix things and show warnings:

```bash
make clang-tidy
```

Issues that will be fixed automatically will be fixed by clang-tidy, warnings will mark any suggestions that clang-tidy couldn't fix directly.

## Test code locally with the Simulator

The simulator compiles in <20s from a clean build, and even less on an incremental build. This makes it fast for us to iterate on anything that doesn't need the physical hardware.

You'll need SDL2 & openssl for it to work: `brew install sdl2 openssl` (on Ubuntu `sudo apt install libsdl2-dev libssl-dev`). Then you can run `make sim` to run it. After building once, you can also debug with lldb at the command line: `lldb lib/Simulator/build/simulator`.

### Extract your Sol cloud access token to download content onto the Simulator

Run the simulator at least once to create your `simprefs.ini` file. Then add the line to that file:

```
cloud-token=<TOKEN>
```

You can get your access token by monitoring logs on the dev board or glasses when pairing with the app. Then you can run "Sync Content" from the simulator just like you would on the glasses.

## Run automated unit tests

This repo has unit tests using [Catch2](https://github.com/catchorg/Catch2). Run them with

```sh
make test
```

The test suite has some simple unit tests as well as view tests. View tests run the app, and record the rendered screens and compare them with known good versions to indicate if anything has changed unexpectedly. If views change the failure messages will tell you how to update the known-good images if necessary.

## Erasing / Resetting Non-Volatile Storage (NVS)

You can erase NVS/Preferences from the command line using esptool. You can either erase the entire internal flash with:

```bash
esptool.py erase_flash
```

Or you can erase a region. The appropriate flash region comes from `esp32s3-64MB.csv`, and it may be worth a check there to confirm this documentation is still up to date. For example, this probably erases NVS:

```bash
esptool.py erase_region 0x9000 0x5000
```

## Extract crash info from core dumps

You can save the most recent core dump when connected over serial. This only works if you still have the `.elf` from the crashing version. With glasses connected, use the following command:

```bash
espcoredump.py -p /dev/tty.usb* info_corefile build/embedded-app.elf
```

Note that if you weren't connected to USB when the crash happened, we may queue the trace to get sent to the server, and it will be unavailable to `espcoredump.py`

See also the [Core Dump docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/core_dump.html).

## Debug with OpenOCD

You can use OpenOCD (included in ESP-IDF) to debug the firmware on the ESP32-S3 using the built-in USB JTAG interface.

Make sure you have `idf.py` set up (see above) and make sure to `idf.py build` the code you want to debug. Then, with your glasses / dev board connect you can run:

```sh
openocd -f board/esp32s3-builtin.cfg
```

Then in a separate terminal, connect gdb:

```sh
xtensa-esp32s3-elf-gdb -x scripts/gdbinit build/embedded-app.elf
```

A _very_ useful GDB command is `catch throw` which sets a breakpoint at any C++ exceptions. You might also try `break filename.cpp:55` to set a breakpoint at a file and line number. Then use `continue` (or `c`) to continue execution until the breakpoint is hit. [Here's a handy GDB cheat sheet](https://users.ece.utexas.edu/~adnan/gdb-refcard.pdf).

You can also use `openocd` to flash firmware over the JTAG interface. I don't think there's any advantage to this vs using `esptool.py`:

```bash
openocd -f board/esp32s3-builtin.cfg -c "program_esp build/embedded-app.bin 0x10000 verify exit"
```

## Hooking up OpenOCD to VSCode

It's possible to use connect to OpenOCD from inside VSCode. You'll need to add a block like this to your `launch.json` file:

```json
{
    "name": "OpenOCD Attach",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/build/embedded-app.elf",
    "MIMode": "gdb",
    // NOTE: This might change as the toolchain changes
    "miDebuggerPath": "${env:HOME}/.espressif/tools/xtensa-esp-elf-gdb/12.1_20221002/xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb",
    "miDebuggerArgs": "${workspaceFolder}/build/embedded-app.elf",

    // This disables adding a breakpoint on `main` though it seems VSCode likes to add it anyways sometimes
    "stopAtEntry": false,

    // Same commands from the scripts/gdbinit file
    "setupCommands": [
        {"text": "target extended-remote :3333"},
        {"text": "set remote hardware-watchpoint-limit 2"},
        {"text": "mon reset halt"},
        {"text": "flushregs"},
        {"text": "thb app_main"},
    ],
    "cwd": "${cwd}",

    // Uncomment to enable logging the GDB commands sent
    // "logging": {
    //   "engineLogging": true
    // },
}
```

Run the `openocd -f board/esp32s3-builtin.cfg` command in the terminal, as above, then hit the "Run -> Start Debugging" button in VSCode. You should see the debugger connect and stop at the `app_main` function.

TODO: This is kinda clunky and it'd be nice to have this easier to setup and run.

## Flashing the device serial number at the factory

Since every individual device has its own unique serial number that can't be changed by the user, we need to burn in the serial number before it arrives at their door. Note that `offset` is in bytes, not bits, and comes from `EFusesCustomTable.csv`.

NOTE: The offset for the serial number is set to 8 bytes because the `burn_block_data` command requires the entire block after the offset to be written, so by offsetting to a right-justified serial number, we can reserve the preceding bits for future use without explicitly burning them as leading zeros.

```bash
# 
echo -n SOL01224400001 > serial_number.bin
espefuse.py --debug --port /dev/cu.usbmodem* --chip esp32s3 burn_block_data --offset 8 BLOCK_USR_DATA serial_number.bin
```

Verify the serial number was fused:

```bash
espefuse.py --port /dev/cu.usbmodem* --chip esp32s3 dump
```

If you change the fuses custom table in `EFusesCustomTable.csv`, you'll need to update the `EFusesCustomTable.c` definitions:

```bash
idf.py efuse-custom-table
```

Note that when you run the above command, the resulting `include/EFusesCustomTable.h` does not have the correct includes. You must add them manually to that file:

```c
#include <esp_efuse.h>
```

This is [fixed](https://github.com/espressif/esp-idf/blob/master/components/efuse/efuse_table_gen.py#L277) in the pre-release version of ESP-IDF 5.0.

## Serial communication protocal for sending commands

The USB serial connection protocol supports file transfers, device control commands, and debugging commands. The key commands include starting file transfers, device programming, restarting the device, retrieving device information, and controlling the device's user interface. On macs, /dev/cu.usbmodemsolreader1 will always be available for accepting commands and file transfers. /dev/cu.usbmodemsolreader3 will be available for logging.

Note that now logging to serial is OFF by default. To easily toggle that once you are connected via usb, issue this command:

```echo "LOGGING_ON" | tee /dev/cu.usbmodemsolreader1```

Command Handling
The protocol is implemented using a command handler that processes incoming commands and performs corresponding actions. The following sections detail each command's functionality.

File Transfer Commands
```START_FILE_TRANSFER```
Initiates a file transfer process.

Action: Sets a passthrough data handler to handle file transfer data chunks.

Device Control Commands
```PROGRAM```
Disables logging and initializes JTAG for programming.

```RESTART```
Sends a success message and restarts the device.

```DEVICE_INFO```
Sends the device's serial number, MAC address, and free space.

```MIRROR```
Toggles the mirroring to the serial. This will send the framebuffer over serial on each display update.

Debug Commands
The debug commands control the user interface for testing purposes. These commands include:

```GRID```: Displays a test alignment screen with maximum brightness.
```UP```: Sends an UP command to the user interface.
```LEFT```: Sends a LEFT command to the user interface.
```RIGHT```: Sends a RIGHT command to the user interface.
```DOWN```: Sends a DOWN command to the user interface.
```ENTER```: Sends an ENTER command to the user interface.
```BACK```: Sends a BACK command to the user interface.
```LOGGING_ON```: Turns on serial logging (on macs, this will be /dev/cu.usbmodemsolreader3)
```LOGGING_OFF```: Turns off serial logging 


## Editing Sol Sans font in the IBMF Font Editor

To change the glyphs in the Sol Sans character map:

1. Download the IBMF Font Editor repo: https://github.com/turgu1/IBMFFontEditor
2. Build the editor:

```sh
mkdir build
cd build
cmake ..
make -j
open IBMFFontEditor.app
```

3. Commit both changes:

    - src/UI/Fonts/IBMFFonts/SolSans_75.h
    - src/UI/Fonts/IBMFFonts/SolSans_75.ibmf

## Generating Bitmaps

A script is included to convert PNG images to XBM format. To use it, you'll need to install the Python requirements (Pillow). I recommend using a Python Virtual Environment to keep your system Python clean.

```bash
python3 -m venv venv
source venv/bin/activate
pip install Pillow
```

Now you can convert all `.png` images in `src/UI/Bitmaps` into `.xbm` files:

```bash
for f in src/UI/Bitmaps/*.png; do echo "Converting $f"; venv/bin/python scripts/image_to_xbm.py "$f" "${f%.png}.xbm"; done
```

If you need to regenerate the app download qr code image, do it like this

```sh
qrencode -s 1 -l Q -m 0 -t PNG32 -o src/UI/Bitmaps/DownloadAppQr.png https://solreader.com/app
```

You might after a while want to clean up unused header bitmap files. You can do that with:

```sh
for f in src/UI/Bitmaps/*.hpp; do b=$(basename $f); git grep -q "Bitmaps/$b" || echo "$b is unused"; done
```

You might also want to trace where the bitmaps are used:

```sh
for f in src/UI/Bitmaps/*.hpp; do b=$(basename $f); echo $f is used in; git grep -l "Bitmaps/$b"; done
```

## Generating static epubs

Strip out non-dithered images (at least until we support grayscale)

```sh
for f in src/Storage/EPubs/*.epub; do \
    echo "Stripping images from $f"; \
    unzip "$f" -d "${f%.epub}"; \
    find "${f%.epub}" -iname "*-greyscale.bmp" | xargs rm; \
    rm "$f"; \
    pushd "${f%.epub}"; \
    zip -r "../$(basename $f)" *; \
    popd; \
    rm -r "${f%.epub}"; \
done
```

```sh
for f in src/Storage/EPubs/*.epub; do echo "Converting $f"; venv/bin/python scripts/file_to_header.py "$f" -o "${f}.hpp"; done
```

## Creating a Release

Simply tag the current commit with a version number like `release/v1.2.3` and push tags. Github Actions will build the firmware and create a release with the firmware binaries attached.

Note: Don't use multiple tags on the same commit or git describe will get confused. Use this command to preview what IDF will use to generate the version string:

```sh
git describe --always --tags --dirty | cut -c 1-31
```

## Adding new build targets

To add a new esp-idf build target you need to do a few things:

* Add a new `TARGET` if statement to the top of the `Makefile` (let's call it `my-new-target` for this example).
* Create a new `sdkconfig.defaults.my-new-target` file.
* Add any new build options as needed to the `src/Kconfig.projbuild` file.
