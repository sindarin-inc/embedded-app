Import("env")
import os

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

hash = os.popen('git rev-parse --short HEAD', 'r', 1).read().strip()

#create header file containing git hash
fp = open("src/Misc/GitHash.hpp", 'w')
fp.write("#ifndef GITHASH_HPP\n")
fp.write("#define GITHASH_HPP\n")
fp.write("const char* githash = \"")
fp.write(hash)
fp.write("\";\n")
fp.write("#endif\n")
fp.close()

env.AddBuildMiddleware(remove_non_simulator_files, "*")
