#include "BootLoopDetection.hpp"

#if SIMULATOR

void BootLoopCheck() {}
void GoodBootTimer() {}

#else

#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <nvs.h>

#define PANIC_BOOT_THRESHOLD 10
#define GOOD_BOOT_THRESHOLD_MS 60000

// check if we are in a boot loop state
// this happens by incrementing a key (curr_boot) on each startup.
// check this against the key last_good_boot. If there are more boots than
// a specified threshold since the last good one, try to roll back to a previous firmware version

auto PartitionIsBootable(const esp_partition_t *partition) -> bool {
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

void Rollback() {
    // has been at least bootLoopThreshold boots since last good one
    // considering this a boot loop, try to fall back to previous firmware.
    log_i("rollback and reset.");

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

auto GetPanicBootCount(nvs_handle_t handle, uint32_t *bootCount) -> bool {
    esp_err_t nvsErr = nvs_get_u32(handle, "panic-boot-count", bootCount);
    if (nvsErr != ESP_OK) {
        log_e("could not get panic-boot-count");
        return false;
    }

    return true;
}

auto OpenBootInfo(nvs_handle_t *handle) -> bool {
    esp_err_t nvsErr = nvs_open("boot-info", NVS_READWRITE, handle);
    if (nvsErr != ESP_OK) {
        log_e("could not open boot-info in nvs");
        return false;
    }

    return true;
}

auto SetPanicBootCount(nvs_handle_t handle, uint32_t bootCount) -> bool {
    esp_err_t nvsErr = nvs_set_u32(handle, "panic-boot-count", bootCount);
    if (nvsErr != ESP_OK) {
        log_e("could not set panic-boot-count to %d", bootCount);
        return false;
    }

    return true;
}

auto EraseAppPreferences() -> bool {
    nvs_handle_t prefsHandle;
    esp_err_t nvsErr = nvs_open("sindarin-app", NVS_READWRITE, &prefsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not open sindarin-app in nvs");
        return false;
    }
    nvsErr = nvs_erase_all(prefsHandle);
    if (nvsErr != ESP_OK) {
        log_e("could not erase prefs");
        nvs_close(prefsHandle);
        return false;
    }

    nvs_close(prefsHandle);
    return true;
}

void BootLoopCheck() {

    // first attempt to open boot-info nvs namespace
    nvs_handle_t bootNvsHandle;
    uint32_t currBoot = 0;
    if (!OpenBootInfo(&bootNvsHandle)) {
        return;
    }

    // read existing boot count
    if (!GetPanicBootCount(bootNvsHandle, &currBoot)) {
        currBoot = 0;
    }

    // check reason for reboot
    if (esp_reset_reason() != ESP_RST_PANIC) {
        // not restarted because of panic, do not need to continue boot loop check
        if (currBoot != 0) {
            // on a normal boot, reset panic boot count back to 0
            currBoot = 0;
            SetPanicBootCount(bootNvsHandle, currBoot);
        }
        nvs_close(bootNvsHandle);
        return;
    }

    // increment current boot number
    currBoot++;

    // check if this should trigger a rollback
    if (currBoot >= PANIC_BOOT_THRESHOLD) {
        // enough panic boots have happened, erase preferences
        // may rollback or do other things here in the future

        // notes from johnboiles on possible actions here:
        /*
        In the future, once we have a way of marking firmwares as either debug or release firmwares,
        I think maybe it'd go like this: For debug builds: after 10 reboots, just fire a loud log
        (BOOT LOOP DETECTED. WAITING FOR NEW FIRMWARE), and wait, and maybe blink an LED. That will
        allow us to update firmware or download a coredump without jamming a paperclip in there. for
        release builds: After 5 reboots, clear the lastReadableId pref After 10 reboots Check an NVS
        key for the most recent good firmware, if this current firmware has never been marked good,
        and there is a previous firmware availabe, then Rollback() If this firmware has been marked
        good in the past, then clear the whole NVS partition. Then re-set the panic-boot-count
        variable. After 15 reboots, kill the whole filesystem
        */

        // reset panic boot count to avoid looping again
        currBoot = 0;
        SetPanicBootCount(bootNvsHandle, currBoot);

        // for now, erase app preferences
        if (!EraseAppPreferences()) {
            log_e("could not erase app preferences");
        }

        nvs_close(bootNvsHandle);
        return;
    }

    // record new boot count
    SetPanicBootCount(bootNvsHandle, currBoot);
    nvs_close(bootNvsHandle);
    return;
}

// timed function which resets panic-boot-count back to 0
void SetGoodBoot(TimerHandle_t timer) {
    log_i("resetting panic-boot-count");

    // open boot info partition
    nvs_handle_t bootNvsHandle;
    if (!OpenBootInfo(&bootNvsHandle)) {
        return;
    }

    // reset number of boots since a 'good' boot to 0
    SetPanicBootCount(bootNvsHandle, 0);
    nvs_close(bootNvsHandle);
}

void GoodBootTimer() {
    // open boot info partition
    nvs_handle_t bootNvsHandle;
    if (!OpenBootInfo(&bootNvsHandle)) {
        return;
    }

    uint32_t bootCount = 0;
    if (!GetPanicBootCount(bootNvsHandle, &bootCount) || bootCount == 0) {
        // no panic boots, no need to reset with a timer
        nvs_close(bootNvsHandle);
        return;
    }
    nvs_close(bootNvsHandle);

    TimerHandle_t bootLoopTimer = xTimerCreate(
        "GoodBootTimer", pdMS_TO_TICKS(GOOD_BOOT_THRESHOLD_MS), pdFALSE, nullptr, SetGoodBoot);
    if (bootLoopTimer == nullptr) {
        log_i("could not create boot loop timer");
    }

    xTimerStart(bootLoopTimer, 0);
}

#endif