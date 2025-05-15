#if CONFIG_FILESYSTEM_SDFAT

#include "SDFatFileSystem.hpp"

#include <driver/sdmmc_host.h>
#include <driver/sdmmc_types.h>
#include <esp_vfs_fat.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>

auto SDFatFileSystem::format() -> bool {
    log_i("Formatting SDFat...");
    DisableCore0Wdt();

    esp_err_t err = esp_vfs_fat_sdcard_format(getPrefix().c_str(), card_);

    EnableCore0Wdt();
    if (err) {
        log_e("Formatting SDFat failed! Error: %d", err);
        return false;
    }
    log_i("Formatting SDFat Complete.");
    return true;
}

auto SDFatFileSystem::begin() -> bool {
    esp_err_t ret;

#if defined(CONFIG_SDCARD_LDO_CHANNEL) && CONFIG_SDCARD_LDO_CHANNEL != -1
    // Initialize the on-chip LDO power control
    sd_pwr_ctrl_ldo_config_t ldoConfig = {
        .ldo_chan_id = CONFIG_SDCARD_LDO_CHANNEL, // Use channel 4 for SD card power
    };
    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldoConfig, &pwrCtrlHandle_);
    if (ret != ESP_OK) {
        log_e("Failed to create on-chip LDO power control driver");
        return false;
    }
#endif
    // Initialize and enable SD card power pin
    gpio_config_t pwrPinConfig = {
        .pin_bit_mask = (1ULL << CONFIG_SDCARD_PWR_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwrPinConfig);
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_SDCARD_PWR_PIN), 0);
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for power to stabilize

    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024,
    };

    // Configure SDMMC host
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_4BIT;
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.io_voltage = 3.3f;
    host.pwr_ctrl_handle = pwrCtrlHandle_;

    // Configure GPIO pins for SDIO
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = static_cast<gpio_num_t>(CONFIG_SDCARD_CLK_PIN);
    slot_config.cmd = static_cast<gpio_num_t>(CONFIG_SDCARD_CMD_PIN);
    slot_config.d0 = static_cast<gpio_num_t>(CONFIG_SDCARD_D0_PIN);
    slot_config.d1 = static_cast<gpio_num_t>(CONFIG_SDCARD_D1_PIN);
    slot_config.d2 = static_cast<gpio_num_t>(CONFIG_SDCARD_D2_PIN);
    slot_config.d3 = static_cast<gpio_num_t>(CONFIG_SDCARD_D3_PIN);
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    const char *mount_point = getPrefix().c_str();
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card_);

    sdmmc_card_print_info(stdout, card_);
    log_i("SD card mounted at %s", mount_point);
    return true;
}

void SDFatFileSystem::close() {
    esp_vfs_fat_sdcard_unmount(getPrefix().c_str(), card_);
    // Turn off SD card power
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_SDCARD_PWR_PIN), 1);
    log_i("SD card unmounted and powered down");
}

SDFatFileSystem::~SDFatFileSystem() {
    if (pwrCtrlHandle_ != nullptr) {
        sd_pwr_ctrl_del_on_chip_ldo(pwrCtrlHandle_);
        pwrCtrlHandle_ = nullptr;
    }
}

auto SDFatFileSystem::getFreeSpace() -> std::string {
    FATFS *fs;
    DWORD freeClusters;
    FRESULT res = f_getfree(getPrefix().c_str(), &freeClusters, &fs);

    if (res != FR_OK) {
        log_e("Failed to get free space: %d", res);
        return "Error";
    }

    uint64_t freeBytes = (uint64_t)freeClusters * fs->csize * 512;
    return std::to_string(freeBytes);
}

#endif // CONFIG_FILESYSTEM_SDFAT
