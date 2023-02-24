#include "LittleFSFileSystem.hpp"

#include <dirent.h>
#include <esp_littlefs.h>
#include <esp_vfs.h>
#include "sindarin-debug.h"

const char* partitionLabel = "data";

bool LittleFSFileSystem::format()
{
    log_i("formatting littlefs..");
    disableCore0WDT();
    esp_err_t err = esp_littlefs_format(partitionLabel);
    enableCore0WDT();
    if (err) {
        log_e("Formatting LittleFS failed! Error: %d", err);
        return false;
    }
    return true;
}

auto LittleFSFileSystem::begin() -> bool {
    // register littlefs filesystem
    esp_vfs_littlefs_conf_t config = {
        .base_path = prefix_.c_str(), .partition_label = partitionLabel, .format_if_mount_failed = false
    };
    esp_err_t err = esp_vfs_littlefs_register(&config);
    if(err == ESP_FAIL ) {
        if (format()) {
            err = esp_vfs_littlefs_register(&config);
        }
    }

    if(err != ESP_OK){
        log_e("Mounting LittleFS failed! Error: %d", err);
        return false;
    }
    return true;
}

void LittleFSFileSystem::close() {
    esp_vfs_littlefs_unregister(partitionLabel);
}