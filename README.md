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
