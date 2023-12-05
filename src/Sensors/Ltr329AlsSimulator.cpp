#if SIMULATOR

#include <cstdio>

#include "Ltr329Als.hpp"
#include "sindarin-debug.h"

auto Ltr329Als::registerReadByte(ltr329als01_reg_t regAddr, uint8_t *data) const -> esp_err_t {
    return ESP_OK;
}

auto Ltr329Als::registerWriteByte(ltr329als01_reg_t regAddr, uint8_t data) const -> esp_err_t {
    return ESP_OK;
}

auto Ltr329Als::readPartId() -> uint8_t { return 0xA0; }

auto Ltr329Als::readManufacturerId() -> uint8_t { return 0x05; }

auto Ltr329Als::enable() -> bool {
    log_d("Enabling Ltr329Als");
    return true;
}

auto Ltr329Als::disable() -> bool {
    log_d("Disabling Ltr329Als");
    return true;
}

auto Ltr329Als::readStatus(Ltr329als01StatusValue &out) -> bool {
    out.alsDataStatus = 1;
    return true;
}

auto Ltr329Als::readData(uint16_t &channel1, uint16_t &channel0) -> bool {
    channel1 = 1000;
    channel0 = 1000;
    return true;
}

#endif
