Import("env")

# It'd typically be good to use `src_filter` in the `platformio.ini` file for this kinda
# thing but that doesn't seem to be able to exclude files from library builds.
def remove_non_simulator_files(node):
    """
    `node.name` - a name of File System Node
    `node.get_path()` - a relative path
    `node.get_abspath()` - an absolute path
    """
    ignore = [
        "ExternalSPIFlash.cpp",
        "SpiffsFileSystem.cpp",
        "Adafruit_SPITFT.cpp",
        "Adafruit_GrayOLED.cpp",
    ]

    if node.name in ignore:
        return None

    return node


env.AddBuildMiddleware(remove_non_simulator_files, "*")


if env["PIOPLATFORM"] == "wasm":
    env.Append(
        LINKFLAGS=["-sUSE_SDL=2"],
    )
    # Override the "upload" target to start a web server hosting the html/js/wasm
    env.AddCustomTarget(
        "upload",
        "$BUILD_DIR/${PROGNAME}${PROGSUFFIX}",
        ">&2 echo 'Starting web server on http://localhost:8080 CTRL-C twice to exit...' && python3 -m http.server -d ${PROJECT_BUILD_DIR}/${PIOENV}/ 8080 & open http://localhost:8080/${PROGNAME}${PROGSUFFIX}",
    )
else:
    # Override the "upload" target to run the local blockware
    env.AddCustomTarget(
        "upload", "$BUILD_DIR/${PROGNAME}", "${PROJECT_BUILD_DIR}/${PIOENV}/${PROGNAME}"
    )
