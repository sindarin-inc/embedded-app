#include "BootLoopDetection.hpp"
#include <Arduino.h>

#if SIMULATOR

void BootLoopCheck() {}
void GoodBootTimer() {}

#else

#include "esp_system.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "Update.h"

#define PANIC_BOOT_THRESHOLD 10

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

void rollback() {
    // has been at least bootLoopThreshold boots since last good one
    // considering this a boot loop, try to fall back to previous firmware.
    log_i("rollback and reset.");

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
}

void BootLoopCheck() {
    // check reason for reboot
    if (esp_reset_reason() != ESP_RST_PANIC) {
        // not restarted because of panic, do not need to continue boot loop check
        return;
    }

    log_i("opened boot-info nvs ok!");

    // increment boot number
    nvs_handle_t bootNvsHandle;
    uint32_t currBoot = 0;
    esp_err_t nvsErr = nvs_open("boot-info", NVS_READWRITE, &bootNvsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not open boot-info in nvs");
        return;
    }

    // read existing boot count
    nvsErr = nvs_get_u32(bootNvsHandle, "panic-boot-count", &currBoot);
    if (nvsErr != ESP_OK) {
        currBoot = 0;
    }

    // increment current boot number
    currBoot++;

    // check if this should trigger a rollback
    if (currBoot >= PANIC_BOOT_THRESHOLD) {
        // enough panic boots have happened, attempt a rollback
        rollback();
    }

    // record new boot count
    nvsErr = nvs_set_u32(bootNvsHandle, "panic-boot-count", currBoot);
    if (nvsErr != ESP_OK) {
        log_e("could not increment panic boot count to %d", currBoot);
    }

    nvs_close(bootNvsHandle);
    return;
}

// timed function which resets panic-boot-count back to 0
void SetGoodBoot(TimerHandle_t timer) {
    log_i("resetting panic-boot-count");

    // open boot info partition
    nvs_handle_t bootNvsHandle;
    esp_err_t nvsErr = nvs_open("boot-info", NVS_READWRITE, &bootNvsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not open boot-info in nvs");
        return;
    }

    // reset number of boots since a 'good' boot to 0
    uint32_t goodBoot = 0;
    nvsErr = nvs_set_u32(bootNvsHandle, "panic-boot-count", goodBoot);
    if (nvsErr != ESP_OK) {
        log_e("could not reset boots since good to 0");
    }

    nvs_close(bootNvsHandle);
}

void BootLoopTimer() {
    // check reason for reboot
    if (esp_reset_reason() != ESP_RST_PANIC) {
        // not restarted because of panic, do not need to launch good boot timer
        return;
    }

    TimerHandle_t bootLoopTimer = xTimerCreate("GoodBootTimer", pdMS_TO_TICKS(10000), pdFALSE, nullptr, SetGoodBoot);
    if (bootLoopTimer == NULL) {
        log_i("could not create boot loop timer");
    }

    xTimerStart(bootLoopTimer, 0);
}

#endif