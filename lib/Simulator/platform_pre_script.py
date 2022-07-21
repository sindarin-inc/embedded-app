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
        "Ltr329Als.cpp",
        "Mmc5603.cpp",
    ]

    if node.name in ignore:
        return None

    return node


env.AddBuildMiddleware(remove_non_simulator_files, "*")
