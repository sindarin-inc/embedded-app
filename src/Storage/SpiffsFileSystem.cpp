#include "SpiffsFileSystem.hpp"

#include <SPIFFS.h>

#include "Storage/ExternalSPIFlash.hpp"
#include "sindarin-debug.h"

void SpiffsFileSystem::begin() {
    SHOW_HEAP();

#if INTERNAL_SPIFFS_FLASH
    const char *partitionLabel = nullptr;
#else
    const char *partitionLabel = "storage";

    // Set up SPI bus and initialize the external SPI Flash chip
    esp_flash_t *flash = SpiFlashInit();
    if (flash == nullptr) {
        log_e("Failed to load external flash");
        return;
    }

    // Add the entire external flash chip as a partition
    SpiFlashAddPartition(flash, partitionLabel, ESP_PARTITION_SUBTYPE_DATA_SPIFFS);
#endif

    // Launch SPIFFS file system
    if (!SPIFFS.begin(false, "/spiffs", 255, partitionLabel)) {
        log_e("An Error has occurred while mounting SPIFFS");
    }

    SHOW_HEAP();
}

auto SpiffsFileSystem::open(const std::string &path) const -> File {
    return SPIFFS.open(path.c_str());
}

auto SpiffsFileSystem::exists(const std::string &path) const -> bool {
    return SPIFFS.exists(path.c_str());
}
