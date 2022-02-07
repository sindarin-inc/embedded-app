# It doesn't appear like there's a way to slot into the default buildfs/uploadfs target
# if for no other reason than these variables only come from the partition table.
# So instead we create new custom targets: buildfs-ext and uploadfs-ext.
# See: https://github.com/platformio/platform-espressif32/blob/master/builder/main.py#L113
Import("env")
from os.path import join


# While we have a 512mbit flash chip on our board, we don't need all of it rn, and it's really slow
# to upload images that big. Let's just pretend we have a 32mbit / 4MB chip.
# See also ExternalSPIFlash.cpp -> SPIFlashInit where size is hardcoded.
flash_size_megabits = 32
flash_size_megabytes = flash_size_megabits / 8
env["SPIFFS_SIZE"] = int(1024 * 1024 * flash_size_megabytes)
env["SPIFFS_SOURCES"] = "data"
env["SPIFFS_START"] = 0
env["SPIFFS_PAGE"] = int("0x100", 16)
env["SPIFFS_BLOCK"] = int("0x1000", 16)
env["ESP32_SPIFFS_IMAGE_NAME"] = "spiffs-external"
spiffs_file = join("$BUILD_DIR", "${ESP32_SPIFFS_IMAGE_NAME}.bin")
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")


build_action = env.VerboseAction(
    " ".join(
        [
            '"$MKSPIFFSTOOL"',
            "-c",
            "$SPIFFS_SOURCES",
            "-p",
            "$SPIFFS_PAGE",
            "-b",
            "$SPIFFS_BLOCK",
            "-s",
            "$SPIFFS_SIZE",
            f'"{spiffs_file}"',
        ]
    ),
    f"Building SPIFFS image from '$SPIFFS_SOURCES' directory to {spiffs_file}",
)

env.AddCustomTarget(
    "buildfs-ext",
    None,
    build_action,
    "Build Filesystem Image (ext)",
    "Build FS for external SPI flash",
)

upload_action = env.VerboseAction(
    " ".join(
        [
            '"$PYTHONEXE" "$OBJCOPY"',
            "--chip",
            mcu,
            "--baud",
            "$UPLOAD_SPEED",
            "--before",
            "default_reset",
            "--after",
            "hard_reset",
            "write_flash",
            "-z",
            "--flash_mode",
            "dio",
            "--flash_size",
            f"{flash_size_megabytes:.0f}MB",
            "--spi-connection",
            "HSPI",
            "0x0",
            f'"{spiffs_file}"',
        ]
    ),
    f"Uploading external SPI flash from {spiffs_file}",
)

env.AddCustomTarget(
    "uploadfs-ext",
    None,
    upload_action,
    "Upload Filesystem Image (ext)",
    "Upload FS to external SPI flash",
)

platform = env.PioPlatform()
env["ESPOTA"] = join(
    platform.get_package_dir("framework-arduinoespressif32") or "", "tools", "espota.py"
)

upload_ota_action = env.VerboseAction(
    " ".join(
        [
            '"$PYTHONEXE" "$ESPOTA"',
            "--debug",
            "--progress",
            "--spiffs",
            "-i",
            env.get("UPLOAD_PORT", "sindarin.local"),
            "-f",
            f'"{spiffs_file}"',
        ]
    ),
    f"Uploading external SPI flash from {spiffs_file} via OTA",
)

env.AddCustomTarget(
    "uploadfs-ota",
    None,
    upload_ota_action,
    "Upload Filesystem Image OTA",
    "Upload FS to external SPI flash via OTA",
)
