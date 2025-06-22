#include <cinttypes>
#include <esp_err.h>
#include <icm42670.h>

class Icm42670 {
public:
    Icm42670() {}

    auto begin(i2c_master_bus_handle_t i2cBusHandle) -> esp_err_t;
    auto end() -> esp_err_t;

    auto readDeviceId(uint8_t *deviceIdOut) -> esp_err_t;
    auto clearInterruptStatus() -> esp_err_t;
    auto configureInterupts() -> esp_err_t;

    auto readAccelerometer(icm42670_value_t *value) -> esp_err_t;

private:
    icm42670_handle_t icm42670_ = nullptr;
};
