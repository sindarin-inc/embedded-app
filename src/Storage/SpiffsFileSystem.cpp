#include "SpiffsFileSystem.hpp"

#include <SPIFFS.h>

#include "sindarin-debug.h"

#if !INTERNAL_SPIFFS_FLASH
#include "Storage/ExternalSPIFlash.hpp"
#endif

auto SpiffsFileSystem::begin() -> bool {
    SHOW_HEAP();

#if !INTERNAL_SPIFFS_FLASH
    const char *partitionLabel = "storage";

    // Set up SPI bus and initialize the external SPI Flash chip
    esp_flash_t *flash = SpiFlashInit();
    if (flash == nullptr) {
        log_e("Failed to load external flash");
        return false;
    }

    // Add the entire external flash chip as a partition
    SpiFlashAddPartition(flash, partitionLabel, ESP_PARTITION_SUBTYPE_DATA_SPIFFS);
#endif

    // Launch SPIFFS file system
    if (!SPIFFS.begin(false, "/spiffs", 255, "data")) {
        log_e("An Error has occurred while mounting SPIFFS");
        return false;
    }

    SHOW_HEAP();
    return true;
}

auto SpiffsFileSystem::open(const std::string &path, const char *mode, const bool create) const
    -> File {
    // Note(john): The version of framework-arduinoespressif32
    // (https://github.com/marcovannoord/arduino-esp32.git#idf-release/v4.0) has a slightly
    // different API here.
    return SPIFFS.open(path.c_str(), mode);
}

auto SpiffsFileSystem::exists(const std::string &path) const -> bool {
    return SPIFFS.exists(path.c_str());
}

auto SpiffsFileSystem::listDir(const std::string &path, FileList &files) const -> int {
    File root = SPIFFS.open(path.c_str());
    File file = root.openNextFile();
    while (file) {
        files.push_back(file.name());
        file = root.openNextFile();
    }
    return 0;
}

auto SpiffsFileSystem::createDir(const std::string &path, bool allowExists) -> bool {
    // SPIFFs does not really have directories
    return true;
}

auto SpiffsFileSystem::remove(const std::string &path) -> bool {
    return SPIFFS.remove(path.c_str());
}
