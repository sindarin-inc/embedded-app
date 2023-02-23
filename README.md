# Sol Reader Glasses Firmware

All the embedded code that runs on the glasses themselves.

## Getting started (build with IDF.py)

After cloning this repo from GitHub you'll need to pull the most recent submodules. You'll need to rerun this command periodically to get the latest changes whenever the submodules change.

```bash
git submodule update --init --recursive
```

The primary way to build the firmware itself is with Espressif's official esp-idf toolchain. We use a [fork of esp-idf](http://github.com/sindarin-inc/esp-idf) that has some patches included. In some other directory (not in this repo), run:

```sh
git clone -b sindarin/v4.4.4 --recursive git@github.com:sindarin-inc/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

If you get errors about `cmake` you might need to install `cmake` first (`brew install cmake` on macOS / `sudo apt install cmake` on Ubuntu).

Now from the root of this repo, you can run (replace `/YOUR/PATH/TO` with the path where you put `esp-idf`):

```bash
source /YOUR/PATH/TO/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build flash monitor
```

This will build the latest firmware, flash it to the device, and open a serial monitor. You can also use `idf.py menuconfig` to configure the build if you need to change build options.

See also the official [esp-idf installation instructions](https://docs.espressif.com/projects/esp-idf/en/v4.4.4/esp32s3/get-started/index.html) for more details

## Install recommended tools

* Visual Studio Code (`brew install visual-studio-code`)
* PlatformIO (`brew install platformio`, or on Ubuntu `sudo apt install python3-pip && pip3 install platformio` or the [installer script](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html)) - we don't typically build the firmware with this but it's needed for the simulator, tests, and clang-tidy.
* clang-format (`brew install clang-format`)
* clang-tidy (`brew install llvm`)

## Reformat code with `clang-format`

Make sure you have `clang-format` installed (`brew install clang-format`). Then if you're using VSCode, and install the recommended workspace extensions, your code should be formatted automatically for you on save (or with shift-option-f). Alternately you can force formatting of all code with:

```bash
find src -iname "*.hpp" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.c" | xargs clang-format -i
```

## Static analysis and cleanup with `clang-tidy`

You can run clang-tidy to do some more advance C++ static analysis and fixes. Make sure you install `clang-tidy` (included with `brew install llvm`) and then run the following to automatically fix things:

```bash
./scripts/clang-tidy -w --fix
```

Issues that will be fixed automatically will be fixed by clang-tidy, warnings will mark any suggestions that clang-tidy couldn't fix directly.

## Test code locally with the Simulator

The simulator compiles in <3s from a clean build, and even less on an incremental build. This makes it super fast for us to iterate on anything that doesn't need the physical hardware.

You'll need SDL2 & openssl for it to work: `brew install sdl2 sdl2_image openssl` (on Ubuntu `sudo apt install libsdl2-dev libsdl2-image-dev libssl-dev`). Then you can run `pio run -e simulator -t upload` to run it. You can alternately hit the upload button in VSCode+PlatformIO and it should build and run! You can also debug with lldb at the command line: `pio run -e simulator && lldb .pio/build/simulator/program`.

### Extract your Sol cloud access token to download content onto the Simulator

Run the simulator at least once to create your `simprefs.ini` file. Then add the line to that file:

```
cloud-token=<TOKEN>
```

You can get your access token by monitoring logs on the dev board or glasses when pairing with the app. Then you can run "Sync Content" from the simulator just like you would on the glasses.

## Run automated unit tests

This repo has unit tests using [Catch2](https://github.com/catchorg/Catch2). Run them with

```sh
pio run -e tests -t upload
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

You can use OpenOCD to debug the firmware on the ESP32-S3 using the built-in JTAG interface. Because OpenOCD out of the box does not support >16MB flash chips until [John's PR](https://github.com/espressif/openocd-esp32/pull/245/files) is merged, you'll need to build a custom version of OpenOCD from [sindarin-inc/openocd-esp32:larger-flashes](https://github.com/sindarin-inc/openocd-esp32/tree/larger-flashes).

First grab dependencies for building

```bash
brew install libtool automake libusb texinfo
brew link texinfo # Override the system texinfo with the brew version
```

Now you can build OpenOCD

```bash
git clone git@github.com:sindarin-inc/openocd-esp32.git
cd openocd-esp32
git checkout larger-flashes
./bootstrap
./configure --enable-esp-usb-jtag
make
```

If all goes well you'll have an `openocd` binary in `src/openocd` inside the repo.

Now save your OpenOCD path to the `OPENOCD_SCRIPTS` variable (used to find board config files and scripts). Also go ahead and add `src` to your path (so you can get to `openocd` later).

```bash
export OPENOCD_SCRIPTS=$(pwd)/tcl
export PATH=$(pwd)/src:$PATH
```

Now go back to the root of the repo, go through the normal process to `idf.py build` and then you can upload using OpenOCD (not sure if uploading with OpenOCD is necessary to debug with OpenOCD but I think so).

```bash
openocd -f board/esp32s3-builtin.cfg -c "program_esp build/embedded-app.bin 0x10000 verify exit"
```

Now you can debug with OpenOCD. First create a debugging session

```bash
openocd -f board/esp32s3-builtin.cfg
```

And then open a separate terminal and connect to the debugger from GDB. Make sure you used the IDF `source export.sh` command to set up your environment variables.

```bash
xtensa-esp32s3-elf-gdb -x scripts/gdbinit build/embedded-app.elf
```

A _very_ useful GDB command is `catch throw` which sets a breakpoint at any C++ exceptions. You might also try `break filename.cpp:55` to set a breakpoint at a file and line number. Then use `continue` (or `c`) to continue execution until the breakpoint is hit. [Here's a handy GDB cheat sheet](https://users.ece.utexas.edu/~adnan/gdb-refcard.pdf).

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
    "miDebuggerPath": "${env:HOME}/.espressif/tools/xtensa-esp32s3-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-gdb",
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
