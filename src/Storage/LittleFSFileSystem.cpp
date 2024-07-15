#include "LittleFSFileSystem.hpp"

#include <dirent.h>
#include <esp_littlefs.h>
#include <esp_vfs.h>

#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "sindarin-debug.h"

const char *partitionLabel = "data";

auto LittleFSFileSystem::format() -> bool {
    log_i("Formatting LittleFS...");
    DisableCore0Wdt();
    PowerManager::getInstance().disableInactivityTimer();

    // TODO: Fix noisy 'task_wdt: esp_task_wdt_reset(783): task not found' log`
    esp_err_t err = esp_littlefs_format(partitionLabel);

    PowerManager::getInstance().enableInactivityTimer();
    EnableCore0Wdt();
    if (err) {
        log_e("Formatting LittleFS failed! Error: %d", err);
        return false;
    }
    log_i("Formatting LittleFS Complete.");
    return true;
}

auto LittleFSFileSystem::begin() -> bool {
    auto prefix = getPrefix();
    // register littlefs filesystem
    esp_vfs_littlefs_conf_t config = {.base_path = prefix.c_str(),
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

auto LittleFSFileSystem::getFreeSpace() -> std::string {
    size_t freeSpace = 0;
    size_t totalBytes = 0;
    size_t usedBytes = 0;

    esp_err_t err = esp_littlefs_info(partitionLabel, &totalBytes, &usedBytes);
    if (err != ESP_OK) {
        log_e("Failed to get LittleFS info: %d", err);
        return "ERROR";
    }
    freeSpace = totalBytes - usedBytes;
    log_i("Got free space: %d", freeSpace);
    return std::to_string(freeSpace);
}