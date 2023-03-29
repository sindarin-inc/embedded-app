#include "LittleFSFileSystem.hpp"

#include <dirent.h>
#include <esp_littlefs.h>
#include <esp_vfs.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

const char *partitionLabel = "data";

auto LittleFSFileSystem::format() -> bool {
    log_i("formatting littlefs..");
    DisableCore0Wdt();
    esp_err_t err = esp_littlefs_format(partitionLabel);
    EnableCore0Wdt();
    if (err) {
        log_e("Formatting LittleFS failed! Error: %d", err);
        return false;
    }
    return true;
}

auto LittleFSFileSystem::begin() -> bool {
    // register littlefs filesystem
    esp_vfs_littlefs_conf_t config = {.base_path = getPrefix(),
                                      .partition_label = partitionLabel,
                                      .format_if_mount_failed = false};
    esp_err_t err = esp_vfs_littlefs_register(&config);
    if (err == ESP_FAIL) {
        if (format()) {
            err = esp_vfs_littlefs_register(&config);
        }
    }

    if (err != ESP_OK) {
        log_e("Mounting LittleFS failed! Error: %d", err);
        return false;
    }
    return true;
}

void LittleFSFileSystem::close() { esp_vfs_littlefs_unregister(partitionLabel); }