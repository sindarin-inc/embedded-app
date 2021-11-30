#!/usr/bin/env python3
import argparse
import os
import pathlib
import subprocess
import sys

parser = argparse.ArgumentParser(description="Generate build flags for simulator.")
parser.add_argument(
    "--wasm",
    action="store_true",
    help="set flags for a web assembly build",
)
args = parser.parse_args()

# Get directory this script is in
script_dir = os.path.dirname(os.path.realpath(__file__))

# Get the lib directory
libs_dir = pathlib.Path(f"{script_dir}/stubs").resolve()
libs = [
    "Arduino",
    "Adafruit_GFX",
]
lib_includes = "\n".join([f"-I{libs_dir}/{lib}" for lib in libs])

# Build the big flags list
flags = [
    "-std=c++1z",
    "-DARDUINO=100",
    "-DSIMULATOR",
    "-DPROGMEM=",
    # "-Dpgm_read_byte(s)='(*(const uint8_t *)(s))'",
    # "-Dpgm_read_byte_near(s)='(*(const uint8_t *)(s))'",
    # "-Dpgm_read_word(s)='(*(const uint16_t *)(s))'",
    # "-Dpgm_read_dword(s)='(*(const uint32_t *)(s))'",
    "-Dheap_caps_malloc(s,l)='((void *)malloc(s))'",
    "-Dheap_caps_realloc(p,s,l)='((void *)realloc(p,s))'",
    "-Dheap_caps_free(p)='( free(p) )'",
    # "-DPGM_P=const char *",
    # "-Dmemcpy_P=memcpy",
    "-DTTRemoteInput_H",
    "-Dnvs_flash_erase()=' '",
    "-Dtypeof(a)='auto '",
    f"-I{script_dir}/stubs",
    lib_includes,
]

if args.wasm:
    flags += ["-sUSE_SDL=2"]
else:
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
    sdl_flags = (
        subprocess.check_output("sdl2-config --cflags --libs", shell=True)
        .decode("utf-8")
        .strip()
    )
    flags += [
        arch_flag,
        sdl_flags,
        f"-I{prefix}/include",
        "--debug",
        # "-glldb",  # Debug
    ]

print(" ".join(flags))
