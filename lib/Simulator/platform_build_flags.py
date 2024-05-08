#!/usr/bin/env python3
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser(description="Generate build flags for simulator.")
parser.add_argument(
    "--wasm",
    action="store_true",
    help="set flags for a web assembly build",
)
parser.add_argument(
    "--sdl",
    action="store_true",
    help="set flags to include SDL",
)
parser.add_argument(
    "--openssl",
    action="store_true",
    help="set flags to include OpenSSL",
)
parser.add_argument(
    "--eightbit",
    action="store_true",
    help="use 8-bit display simulation",
)
args = parser.parse_args()

# Get directory this script is in
script_dir = os.path.dirname(os.path.realpath(__file__))

# Get the lib directory
libs_dir = pathlib.Path(f"{script_dir}/stubs").resolve()
libs = [
    "Arduino",
    "esp-idf",
    "freertos-posix",
    "nvs_flash",
]
lib_includes = "\n".join([f"-I{libs_dir}/{lib}" for lib in libs])

components_dir = pathlib.Path(f"{script_dir}/../../components").resolve()
component_libs = [
    "Adafruit-GFX-Library",
    "ArduinoJson",
]

lib_includes += "\n" + "\n".join([f"-I{components_dir}/{lib}" for lib in component_libs])

# Build the big flags list
flags = [
    "-std=c++17",
    "-DTTRemoteInput_H",
    "-Dtypeof(a)='auto '",
    "-DSIMULATOR=1",
    "-DUSE_TURN_TOUCH_REMOTE=0",
    "-DUSE_BLE_SERVER_REMOTE=0",
    "-DUSE_BLE_SERVER_SETTINGS=0",
    "-DUSE_BLE_SERVER_IMPROV=0",
    "-DRTC_SLOW_ATTR=",
    "-DENABLE_OTA=0",
    "-lpthread",
    "-Isrc",
    lib_includes,
]

if args.eightbit:
    flags += [
        "-DDISPLAY_SIM_8BIT=1",
        "-DEINKET013TT1=0",
    ]
else:
    flags += [
        "-DEINKET013TT1=1",
    ]

if args.wasm:
    flags += [
        "-sUSE_SDL=2",
    ]

if args.sdl:
    # Native builds need to know how to find SDL2 and may need to set the correct architecture
    # Find SDL2
    try:
        out = subprocess.check_output(["sdl2-config", "--prefix"])
    except FileNotFoundError:
        print(
            "Could not find SDL2. Maybe install it? (brew install SDL2)",
            file=sys.stderr,
        )
        exit(1)

    prefix = pathlib.Path(out.decode("utf-8").strip())

    # Find the arch of installed SDL2 (macOS only to support Apple Silicon)
    # Sometimes for some reason, platformio tries to build native as x86_64
    # even on arm. Not totally sure why that is.
    arch_flag = ""
    if sys.platform == "darwin":
        out = subprocess.check_output(
            f"lipo -info {prefix}/lib/libSDL2.a | awk '{{ print $NF }}'", shell=True
        )
        arch = out.decode("utf-8").strip()
        arch_flag = f"-arch {arch}"

    # Get the needed SDL flags
    sdl_flags = subprocess.check_output("sdl2-config --cflags --libs", shell=True).decode("utf-8").strip()
    flags += [
        arch_flag,
        sdl_flags,
        f"-I{prefix}/include",
        "--debug",
    ]

if args.openssl:
    # Ubuntu defaults
    openssl_flags = "-I/usr/include/openssl"

    # Try with macOS Homebrew to find OpenSSL
    if sys.platform == "darwin" and shutil.which("brew") is not None:
        # TODO: Handle error
        openssl_prefix = subprocess.check_output("brew --prefix openssl", shell=True).decode("utf-8").strip()
        try:
            subprocess.check_output(f"ls {openssl_prefix}", shell=True)
        except subprocess.CalledProcessError:
            print(
                "Could not find openssl. Maybe install it? (brew install openssl)",
                file=sys.stderr,
            )
            exit(1)

        openssl_flags = f"-L{openssl_prefix}/lib -I{openssl_prefix}/include"

    flags += [
        "-DCPPHTTPLIB_OPENSSL_SUPPORT",
        openssl_flags,
        "-lssl -lcrypto",
    ]


ret = subprocess.run(["git", "describe", "--always", "--tags", "--dirty"], stdout=subprocess.PIPE, text=True)
git_version = ret.stdout.strip()[:31]
flags += [
    f'-DGIT_VERSION=\\"{git_version}\\"',
]

print(" ".join(flags))
