#include "SDFatFileSystem.hpp"

#include <esp_vfs_fat.h>

#include "Misc/PowerManager.hpp"

#define CONFIG_EXAMPLE_PIN_MOSI 15
#define CONFIG_EXAMPLE_PIN_MISO 16
#define CONFIG_EXAMPLE_PIN_CLK 11
#define CONFIG_EXAMPLE_PIN_CS 42

auto SDFatFileSystem::format() -> bool {
    log_i("Formatting SDFat...");
    DisableCore0Wdt();
    PowerManager::getInstance().disableInactivityTimer();

    esp_err_t err = esp_vfs_fat_sdcard_format(getPrefix().c_str(), card);

    PowerManager::getInstance().enableInactivityTimer();
    EnableCore0Wdt();
    if (err) {
        log_e("Formatting SDFat failed! Error: %d", err);
        return false;
    }
    log_i("Formatting SDFat Complete.");
    return true;
}

static esp_err_t s_example_write_file(const char *path, char *data) {
    log_i("Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        log_e("Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    log_i("File written");

    return ESP_OK;
}

auto SDFatFileSystem::begin() -> bool {
    esp_err_t ret;

    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = static_cast<gpio_num_t>(CONFIG_EXAMPLE_PIN_MOSI),
        .miso_io_num = static_cast<gpio_num_t>(CONFIG_EXAMPLE_PIN_MISO),
        .sclk_io_num = static_cast<gpio_num_t>(CONFIG_EXAMPLE_PIN_CLK),
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    auto slot = static_cast<spi_host_device_t>(host.slot);

    ret = spi_bus_initialize(slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        log_e("Failed to initialize SPI bus");
        return false;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = static_cast<gpio_num_t>(CONFIG_EXAMPLE_PIN_CS);
    slot_config.host_id = slot;

    const char *mount_point = getPrefix().c_str();
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        log_e("Failed to mount filesystem: %s", esp_err_to_name(ret));
        spi_bus_free(slot);
        return false;
    }

    sdmmc_card_print_info(stdout, card);

    log_i("SD card mounted at %s", mount_point);
    return true;
}

void SDFatFileSystem::close() {
    esp_vfs_fat_sdcard_unmount(getPrefix().c_str(), card);
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    auto slot = static_cast<spi_host_device_t>(host.slot);
    spi_bus_free(slot);
    log_i("SD card unmounted");
}

auto SDFatFileSystem::getFreeSpace() -> std::string {
    // TODO: Implement it
    return "Unimplemented";
}
