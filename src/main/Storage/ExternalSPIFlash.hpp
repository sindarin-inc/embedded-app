#include <esp_flash.h>
#include <esp_partition.h>

auto SpiFlashInit() -> esp_flash_t *;

auto SpiFlashAddPartition(esp_flash_t *extFlash, const char *partitionLabel,
                          esp_partition_subtype_t subtype) -> const esp_partition_t *;

void SpiFlashListPartitions();
