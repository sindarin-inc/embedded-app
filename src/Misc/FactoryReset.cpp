#include "FactoryReset.hpp"

#include <esp_partition.h>
#include <nvs_flash.h>

#include "sindarin-debug.h"

void SoftFactoryReset(FileSystem &fs) {
    log_e("Performing soft factory reset");
    fs.deleteAll();
    nvs_flash_erase();
}

void HardFactoryReset() {
    log_e("Performing hard factory reset");
#if !SIMULATOR
    auto partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "data");
    if (partition == nullptr) {
        log_e("Failed to find filesystem partition");
    } else {
        auto err = esp_partition_erase_range(partition, 0, partition->size);
        if (err != ESP_OK) {
            log_e("Failed to erase filesystem partition");
        } else {
            log_i("Erased filesystem partition");
        }
    }
#endif
    nvs_flash_erase();
}
