#if !CONFIG_INTERNAL_SPIFFS_FLASH

#include "Storage/ExternalSPIFlash.hpp"

#include <driver/spi_common.h>
#include <esp32-hal-log.h>
#include <esp_flash.h>
#include <esp_flash_spi_init.h>
#include <esp_partition.h>
#include <esp_spiffs.h>
#include <esp_vfs.h>

auto SpiFlashInit() -> esp_flash_t * {
    // HSPI / SPI2
    const spi_bus_config_t busConfig = {
        .mosi_io_num = HSPI_IOMUX_PIN_NUM_MOSI, // IO13
        .miso_io_num = HSPI_IOMUX_PIN_NUM_MISO, // IO12
        .sclk_io_num = HSPI_IOMUX_PIN_NUM_CLK,  // IO14
        // .quadwp_io_num = HSPI_IOMUX_PIN_NUM_WP, // IO2
        // .quadhd_io_num = HSPI_IOMUX_PIN_NUM_HD, // IO4
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    const esp_flash_spi_device_config_t deviceConfig = {.host_id = HSPI_HOST,
                                                        .cs_io_num = HSPI_IOMUX_PIN_NUM_CS, // IO15
                                                        .io_mode = SPI_FLASH_DIO,
                                                        // These modes should work with our flash
                                                        //  .io_mode = SPI_FLASH_QOUT,
                                                        //  .io_mode = SPI_FLASH_QIO,
                                                        .speed = ESP_FLASH_40MHZ};

    log_d("Initializing external SPI Flash");
    log_d("Pin assignments:\nSCLK: %2d MISO: %2d MOSI: %2d CS: %2d", busConfig.mosi_io_num,
          busConfig.miso_io_num, busConfig.sclk_io_num, deviceConfig.cs_io_num);

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &busConfig, 1));

    // Add device to the SPI bus
    esp_flash_t *extFlash;
    ESP_ERROR_CHECK(spi_bus_add_flash_device(&extFlash, &deviceConfig));

    // Probe the Flash chip and initialize it
    esp_err_t err = esp_flash_init(extFlash);
    if (err != ESP_OK) {
        log_e("Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(err), err);
        return nullptr;
    }

    // Print out the ID and size
    uint32_t id;
    ESP_ERROR_CHECK(esp_flash_read_id(extFlash, &id));
    // For some reason, we can't auto-detect the size of 512mbit flash chips
    if (id == 0xef4020) {
        // While we have a 512mbit flash chip on our board, we don't need all of it rn, and it's
        // really slow to upload images that big. Let's just pretend we have a 32mbit / 4MB chip.
        // See also: pio_external_flash.py where this is also hardcoded.
        // ext_flash->size = 1024 * 1024 * 512 / 8;
        extFlash->size = 1024 * 1024 * 128 / 8;
        log_i("Detected 512mbit (64MB) flash ID=0x%x, hardcoding size to %d", id, extFlash->size);
    }
    log_i("Initialized external Flash, size=%d KB, ID=0x%x", extFlash->size / 1024, id);

    return extFlash;
}

auto SpiFlashAddPartition(esp_flash_t *extFlash, const char *partitionLabel,
                          esp_partition_subtype_t subtype) -> const esp_partition_t * {
    size_t size = extFlash->size;
    log_d("Adding external Flash as a partition, label=\"%s\", size=%d KB", partitionLabel,
          size / 1024);
    const esp_partition_t *partition;
    const int offset = 0x0;
    ESP_ERROR_CHECK(esp_partition_register_external(extFlash, offset, size, partitionLabel,
                                                    ESP_PARTITION_TYPE_DATA, subtype, &partition));
    return partition;
}

void SpiFlashListPartitions() {
    log_i("Listing data partitions:");
    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    for (; it != nullptr; it = esp_partition_next(it)) {
        const esp_partition_t *part = esp_partition_get(it);
        log_i("- chip '%d' partition '%s', subtype 0x%02x, offset 0x%x, size %d kB",
              part->flash_chip, part->label, part->subtype, part->address, part->size);
    }

    esp_partition_iterator_release(it);
}

#endif