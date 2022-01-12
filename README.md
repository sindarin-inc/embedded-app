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
