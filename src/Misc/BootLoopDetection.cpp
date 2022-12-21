#include "BootLoopDetection.hpp"

#if SIMULATOR

uint32_t BootLoopCheck() {
    return 1;
}

void SetLastGoodBoot(uint32_t lastGood) {
}

#else

#include "esp_ota_ops.h"
#include "nvs.h"
#include "Update.h"

// check if we are in a boot loop state
// this happens by incrementing a key (curr_boot) on each startup.
// check this against the key last_good_boot. If there are more boots than
// a specified threshold since the last good one, try to roll back to a previous firmware version

bool partitionIsBootable(const esp_partition_t *partition) {
    uint8_t buf[ENCRYPTED_BLOCK_SIZE];
    if (!partition) {
        return false;
    }
    if (!ESP.partitionRead(partition, 0, (uint32_t *)buf, ENCRYPTED_BLOCK_SIZE)) {
        return false;
    }

    if (buf[0] != ESP_IMAGE_HEADER_MAGIC) {
        return false;
    }
    return true;
}

uint32_t BootLoopCheck() {
    // increment boot number
    nvs_handle_t bootNvsHandle;
    uint32_t currBoot = 0;
    uint32_t bootLoopThreshold = 10;
    esp_err_t nvsErr = nvs_open("boot-info", NVS_READWRITE, &bootNvsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not open boot-info in nvs");
        return 0;
    }

    log_i("opened boot-info nvs ok!");

    // read existing boot count
    nvsErr = nvs_get_u32(bootNvsHandle, "curr_boot", &currBoot);
    if (nvsErr != ESP_OK) {
        currBoot = 0;
    }

    // increment current boot number
    currBoot++;
    log_i("setting curr boot to %d", currBoot);
    if (nvs_set_u32(bootNvsHandle, "curr_boot", currBoot) != ESP_OK) {
        log_e("error setting curr boot to %d", currBoot);
        nvs_close(bootNvsHandle);
        return currBoot;
    }

    // now read last good boot number
    uint32_t lastGoodBoot = 0;
    nvsErr = nvs_get_u32(bootNvsHandle, "last_good_boot", &lastGoodBoot);
    if (nvsErr != ESP_OK) {
        log_e("error reading last good boot");
        nvs_close(bootNvsHandle);
        return currBoot;
    }

    if (currBoot <= lastGoodBoot) {
        log_e("error, last good boot %d > curr boot %d", lastGoodBoot, currBoot);
        nvs_close(bootNvsHandle);
        return currBoot;
    }

    uint32_t bootsSinceGood = currBoot - lastGoodBoot;
    log_i("curr boot %d last good %d since good %d, thresh %d", currBoot, lastGoodBoot,
          bootsSinceGood, bootLoopThreshold);
    // check if it has been a while since last good boot

    if (bootsSinceGood <= bootLoopThreshold) {
        nvs_close(bootNvsHandle);
        return currBoot;
    }
    // has been at least bootLoopThreshold boots since last good one
    // considering this a boot loop, try to fall back to previous firmware.
    log_i("rollback and reset.");

    // set this boot as 'good' so rolled back version doesn't also try to roll back!
    nvsErr = nvs_set_u32(bootNvsHandle, "last_good_boot", currBoot);
    if (nvsErr != ESP_OK) {
        log_e("error setting last good boot to %d", currBoot);
        nvs_close(bootNvsHandle);
        return currBoot;
    }

    const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
    if (partition && partitionIsBootable(partition)) {
        if (esp_ota_set_boot_partition(partition) == ESP_OK) {
            // reboot to other partition
            esp_restart();
        } else {
            log_e("error: cannot roll back, cannot set boot partition");
        }
    } else {
        log_e("error: cannot roll back, partition is not bootable.");
    }

    nvs_close(bootNvsHandle);
    return currBoot;
}

void SetLastGoodBoot(uint32_t lastGood) {
    nvs_handle_t bootNvsHandle;
    esp_err_t nvsErr = nvs_open("boot-info", NVS_READWRITE, &bootNvsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not open boot-info in nvs");
        return;
    }

    // mark this boot as good
    log_i("marking %d as last good boot", lastGood);
    nvsErr = nvs_set_u32(bootNvsHandle, "last_good_boot", lastGood);
    if (nvsErr != ESP_OK) {
        log_e("could not set %d as last good boot", lastGood);
    }

    nvs_close(bootNvsHandle);
}

#endif