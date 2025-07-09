#pragma once

#include <cinttypes>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>

class Sths34pf80 {
public:
    Sths34pf80() {}

    auto begin(i2c_master_bus_handle_t i2cBusHandle) -> esp_err_t;
    auto end() -> esp_err_t;
    auto powerDown() -> esp_err_t;

    auto readDeviceId(uint8_t *deviceIdOut) -> esp_err_t;
    auto configureInterrupts() -> esp_err_t;
    auto checkPresence(bool *presenceDetected) -> esp_err_t;
    auto clearInterruptStatus() -> esp_err_t;

private:
    i2c_master_dev_handle_t dev_handle_ = nullptr;
};