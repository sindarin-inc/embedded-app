#include "BootLoopDetection.hpp"

#include "Misc/FactoryReset.hpp"
#include "Misc/Platform.hpp"

#if SIMULATOR

void BootLoopCheck(FileSystem &fs, PowerManager &pm) {}
void GoodBootTimer() {}
auto GetBadBootCount() -> uint8_t { return 0; };

#else

RTC_NOINIT_ATTR uint8_t badBootCount;

#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <nvs.h>

#include "sindarin-debug.h"

#define GOOD_BOOT_THRESHOLD_MS 60000

auto PartitionIsBootable(const esp_partition_t *partition) -> bool {
    uint8_t buf[ENCRYPTED_BLOCK_SIZE];
    if (!partition) {
        return false;
    }
    if (!esp_partition_read(partition, 0, buf, ENCRYPTED_BLOCK_SIZE)) {
        return false;
    }

    if (buf[0] != ESP_IMAGE_HEADER_MAGIC) {
        return false;
    }
    return true;
}

void Rollback() {
    log_e("Rolling back to previous firmware");

    const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
    if (partition && PartitionIsBootable(partition)) {
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

void EraseCurrentReadable() {
    log_e("Resetting current readable");

    nvs_handle_t prefsHandle;
    esp_err_t nvsErr = nvs_open("sindarin-app", NVS_READWRITE, &prefsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not open sindarin-app in nvs");
        return;
    }
    nvsErr = nvs_erase_key(prefsHandle, "rui-readable");
    if (nvsErr != ESP_OK) {
        log_e("could not erase rui-readable");
    }
    nvs_close(prefsHandle);
}

void BootLoopCheck(FileSystem &fs, PowerManager &pm) {
    auto resetReason = esp_reset_reason();
    if (resetReason != ESP_RST_PANIC && resetReason != ESP_RST_INT_WDT &&
        resetReason != ESP_RST_TASK_WDT) {
        if (badBootCount != 0) {
            log_i("Good boot after %d bad boots", badBootCount);
            badBootCount = 0;
        }
        return;
    }

    // increment current boot number
    badBootCount++;

    log_e("%d bad boots!", badBootCount);

    if (badBootCount >= 18) {
        Rollback();
    } else if (badBootCount >= 15) {
        HardFactoryReset();
    } else if (badBootCount >= 12) {
        // Blow everything away
        fs.begin();
        SoftFactoryReset(fs);
        // It might not be good to start the filesystem here, then proceed, so to be safe let's
        // reboot
        pm.reboot();
    } else if (badBootCount >= 9) {
        // Erase the filesystem
        log_e("Deleting all files");
        fs.deleteAll();
    } else if (badBootCount >= 3) {
        // Unset the current readable
        EraseCurrentReadable();
    }
}

// timed function which resets panic-count back to 0
void SetGoodBoot(TimerHandle_t timer) {
    log_i("Good Boot! Resetting badBootCount");
    // reset number of boots since a 'good' boot to 0
    badBootCount = 0;
}

void GoodBootTimer() {
    if (badBootCount == 0) {
        // no panic boots, no need to reset with a timer
        return;
    }

    TimerHandle_t bootLoopTimer = xTimerCreate(
        "GoodBootTimer", pdMS_TO_TICKS(GOOD_BOOT_THRESHOLD_MS), pdFALSE, nullptr, SetGoodBoot);
    if (bootLoopTimer == nullptr) {
        log_i("could not create boot loop timer");
    }

    xTimerStart(bootLoopTimer, 0);
}

auto GetBadBootCount() -> uint8_t { return badBootCount; };

#endif