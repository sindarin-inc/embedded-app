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
        "CryptoMbedTls.cpp",
        "PlatformESP32.cpp",
        "SindarinWiFi.cpp",
        "Updater.cpp",
        "interrupt-hal.cpp",
        "ExternalSPIFlash.cpp",
        "LittleFSFileSystem.cpp",
        "Adafruit_SPITFT.cpp",
        "Adafruit_GrayOLED.cpp",
        "Ltr329Als.cpp",
        "Mmc5603.cpp",
        "TempleCloseSensor.cpp",
        "WiFi.cpp",
        "main.c",
    ]

    if node.name in ignore:
        return None

    return node


def ignore_node_path_warnings(env, node):
    """
    Ignore -Wdeprecated-non-prototype warnings for zlib-PIO

    See https://github.com/madler/zlib/issues/633

    `node.name` - a name of File System Node
    `node.get_path()` - a relative path
    `node.get_abspath()` - an absolute path
    """

    if "zlib-PIO" in node.get_path():
        return env.Object(
            node,
            CPPDEFINES=env["CPPDEFINES"],
            CCFLAGS=env["CCFLAGS"] + ["-Wno-deprecated-non-prototype"],
        )
    else:
        return node


env.AddBuildMiddleware(remove_non_simulator_files, "*")
env.AddBuildMiddleware(ignore_node_path_warnings, "*.c")
