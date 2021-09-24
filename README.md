# Sindarin Embedded Firmware

## Getting started

Install platformio

```bash
brew install platformio
```

VSCode provides a nice platformio IDE. Download VSCode and get the PlatformIO IDE extension.

## Upload and Monitor

You can use the "Upload and Monitor" project tasks under PlatformIO Icon (left bar) -> Project Tasks -> tinypico -> General -> Upload and Monitor

You'll also need to build and upload the filesystem. You can do this in VSCode from PlatformIO Icon (left bar) -> Project Tasks -> tinypico -> Platform -> Build Filesystem Image and then Upload Filesystem Image. Or you can do this at the command line with `pio run --target uploadfs`.

Note: If you don't see these options in your PlatformIO bar in VSCode, try restarting VSCode.

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
