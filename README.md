# Sindarin Embedded Firmware

## Getting started

Install platformio

```bash
brew install platformio
```

After cloning this project from GitHub you'll need to pull the most recent submodules if you want to build the IDF builds.

```bash
git submodule update --init --recursive
```

VSCode provides a nice platformio IDE. Download VSCode and get the PlatformIO IDE extension.

## Upload and Monitor

You can use the "Upload and Monitor" project tasks under PlatformIO Icon (left bar) -> Project Tasks -> sol3dev -> General -> Upload and Monitor

Note: You'll probably get a boot loop after uploading because of the flash memory size detected as 0. You'll need to apply [my IDF patch](https://github.com/espressif/esp-idf/pull/9381) to fix flashes > 512mbit in the bootloader. Find `spi_flash_chip_generic.c` (probably in `~/.platformio/packages/framework-espidf/components/spi_flash/spi_flash_chip_generic.c`) and apply my change from that PR. You'll also very likely need to apply the changes from [this commit](https://github.com/espressif/esp-idf/commit/04959af6354ba28b5559f39ccb339b49521dc114) (this will no longer be necessary once PlatformIO updates to IDF 4.4.2).

## Add Readables to SPIFFS

To put Readables on the device, you'll need to collect some readables. Perhaps from the [sindarin-inc/readables](https://github.com/sindarin-inc/readables) repo. Just download the Readables you want and put them in a directory in the root of this repo named `data`.

Then you'll need to build and upload the filesystem. You can do this in VSCode from PlatformIO Icon (left bar) -> Project Tasks -> sol3dev -> Platform -> Upload Filesystem Image. Or you can do this at the command line with `pio run -t uploadfs`.

Note: If you don't see these options in your PlatformIO bar in VSCode, try restarting VSCode.

## Upload Over-the-Air (OTA)

You can upload the firmware and filesystem over-the-air. You'll need to make sure to connect to WiFi. Then you can use this to upload the firwmare OTA:

```bash
pio run -t upload --upload-port sindarin.local
```

An this to upload the SPIFFS image:

```
pio run -t uploadfs-ota --upload-port sindarin.local
```

## Git Setup

You probably want to ignore formatting changes when using `git blame`. After cloning, you can tell Git to ignore certain commits with:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## Reformat Code

Make sure you have `clang-format` installed (`brew install clang-format`). Then if you're using VSCode, and install the recommended workspace extensions, your code should be formatted automatically for you on save (or with shift-option-f). Alternately you can force formatting of all code with:

```bash
find src -iname "*.hpp" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.c" | xargs clang-format -i

## Using ESP-IDF Menuconfig

You can use the ESP-IDF menuconfig using

```
pio run -t menuconfig
```

You may need to first update to a more modern version of `flex`

```
brew install flex
export PATH="/opt/homebrew/opt/flex/bin:$PATH"
pio run -t menuconfig
```

## Clang-tidy

You can run clang-tidy to do some more advance C++ static analysis and fixes. Make sure you install `clang-tidy` (included with `brew install llvm`) and then run the following to automatically fix things:

```bash
./scripts/clang-tidy -w --fix
```

Issues that will be fixed automatically will be fixed by clang-tidy, warnings will mark any suggestions that clang-tidy couldn't fix directly.

## Simulator!

The simulator compiles in <3s from a clean build, and even less on an incremental build. Thsi makes it super fast for us to iterate on anything that doesn't need the physical hardware.

You'll need SDL2 & openssl for it to work: `brew install sdl2 sdl2_image openssl`. But then you can just hit the upload button in VSCode+PlatformIO and it should build and run! We haven't figured out how to get VSCode to properly debug the binary yet, but for now you can use the command line: `pio run -e simulator && lldb .pio/build/simulator/program`. Enjoy!

To run any of the testsuite stuff, you'll need a very specific version of the `sdl2_image` package (2.0.5), because latest versions have a bug in reading in RGB values from pngs that we haven't debugged and fixed yet. Do a `brew info sdl2_image` -- if it's not 2.0.5, then do the following:

`brew uninstall sdl2_image`

`find $(brew --prefix) -name sdl2_image.rb`

That should show you where the current formula lives. Replace that file with the formula found in `lib/Simulator/sdl2_image.rb`

The magic incatation to do it all is:

`find $(brew --prefix) -name sdl2_image.rb | xargs -I {} cp lib/Simulator/sdl2_image.rb {}`

Then run: `brew install sdl2_image`

You can then confirm you have version 2.0.5 with `brew info sdl2_image`

## Erasing / Resetting Non-Volatile Storage (NVS)

You can erase NVS/Preferences from the command line using esptool. You can either erase the entire internal flash with:

```bash
esptool.py erase_flash
```

Or you can erase a region. The appropriate flash region comes from [`externalflash.csv`](https://github.com/sindarin-inc/embedded-app/blob/main/externalflash.csv), and it may be worth a check there to confirm this documentation is still up to date.

```bash
esptool.py erase_region 0x9000 0x5000
```

## Core dumps

You can save the most recent core dump when connected over serial. This probably only works if you still have the `.elf` from the crashing version, though more experimentation is needed. Use the following command:

```bash
espcoredump.py -p /dev/tty.usb* info_corefile .pio/build/sol3dev/firmware.elf
```

See also the [Core Dump docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/core_dump.html).

## Build with IDF

This repo also supports building with IDF.py. You need to first download and set up IDF through the [usual channels](https://docs.espressif.com/projects/esp-idf/en/v4.4.1/esp32/get-started/index.html). Ideally match the version of IDF used in PlatformIO (see the `common:idf` section in `platformio.ini`).

You'll also need to apply [my IDF patch](https://github.com/espressif/esp-idf/pull/9381) to fix Winbond flashes > 512mbit in the bootloader.

Then you can build the firmware with:

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```
