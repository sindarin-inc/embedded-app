# Sindarin Embedded Firmware

## Getting started

Install platformio

```bash
brew install platformio
```

VSCode provides a nice platformio IDE. Download VSCode and get the PlatformIO IDE extension.

## Upload and Monitor

You can use the "Upload and Monitor" project tasks under PlatformIO Icon (left bar) -> Project Tasks -> gilpico -> General -> Upload and Monitor

## Add Readables to SPIFFS

To put Readables on the device, you'll need to collect some readables. Perhaps from the [sindarin-inc/readables](https://github.com/sindarin-inc/readables) repo. Just download the Readables you want and put them in a directory in the root of this repo named `data`.

Then you'll need to build and upload the filesystem. You can do this in VSCode from PlatformIO Icon (left bar) -> Project Tasks -> gilpico -> Custom -> Build Filesystem Image (ext) and then Upload Filesystem Image (ext). Or you can do this at the command line with `pio run -t buildfs-ext -t uploadfs-ext`.

Note: If you don't see these options in your PlatformIO bar in VSCode, try restarting VSCode.

## Upload Over-the-Air (OTA)

You can upload the firmware and filesystem over-the-air. You'll need to make sure to connect to WiFi. Then you can use this to upload the firwmare OTA:

```bash
pio run -e gilpico -t upload --upload-port sindarin.local
```

An this to upload the SPIFFS image:

```
pio run -e gilpico -t uploadfs-ota --upload-port sindarin.local
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
