#include "LittleFSFileSystem.hpp"

#include <dirent.h>
#include <esp_littlefs.h>
#include <esp_vfs.h>

#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "sindarin-debug.h"

const char *partitionLabelPreload = "predata";
const char *partitionLabelUser = "userdata";

auto LittleFSFileSystem::format() -> bool {
    log_i("Formatting LittleFS...");
    DisableCore0Wdt();
    PowerManager::getInstance().disableInactivityTimer();

    // TODO: Fix noisy 'task_wdt: esp_task_wdt_reset(783): task not found' log`
    esp_err_t err = esp_littlefs_format(partitionLabelUser);

    PowerManager::getInstance().enableInactivityTimer();
    EnableCore0Wdt();
    if (err) {
        log_e("Formatting LittleFS failed! Error: %d", err);
        return false;
    }
    log_i("Formatting LittleFS Complete.");
    return true;
}

auto LittleFSFileSystem::attemptMount(const std::string &prefix, const char *partitionLabel)
    -> esp_err_t {
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
        log_e("Mounting LittleFS on partition '%s' with prefix '%s' failed! Error: %d",
              partitionLabel, prefix.c_str(), err);
        return err;
    }
    return ESP_OK;
}

auto LittleFSFileSystem::begin() -> bool {
    if (attemptMount(getPrefixPreload(), partitionLabelPreload) != ESP_OK) {
        return false;
    }
    if (attemptMount(getPrefix(), partitionLabelUser) != ESP_OK) {
        return false;
    }
    return true;
}

void LittleFSFileSystem::close() {
    esp_vfs_littlefs_unregister(partitionLabelPreload);
    esp_vfs_littlefs_unregister(partitionLabelUser);
}