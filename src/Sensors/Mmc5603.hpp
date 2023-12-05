#include <cinttypes>
#include <esp_err.h>

enum mmc5603_reg_t {
    MMC5603_REG_XOUT_0 = 0x00,
    MMC5603_REG_XOUT_1 = 0x01,
    MMC5603_REG_YOUT_0 = 0x02,
    MMC5603_REG_YOUT_1 = 0x03,
    MMC5603_REG_ZOUT_0 = 0x04,
    MMC5603_REG_ZOUT_1 = 0x05,
    MMC5603_REG_XOUT_2 = 0x06,
    MMC5603_REG_YOUT_2 = 0x07,
    MMC5603_REG_ZOUT_2 = 0x08,
    // Temperature output, unsigned format. The range is -75-125°C, about 0.8°C/LSB, 00000000 stands
    // for -75°C
    MMC5603_REG_TOUT = 0x09,
    MMC5603_REG_STATUS_1 = 0x18,
    MMC5603_REG_ODR = 0x1A,
    MMC5603_REG_CTRL_0 = 0x1B,
    MMC5603_REG_CTRL_1 = 0x1C,
    MMC5603_REG_CTRL_2 = 0x1D,
    MMC5603_REG_ST_X_TH = 0x1E,
    MMC5603_REG_ST_Y_TH = 0x1F,
    MMC5603_REG_ST_Z_TH = 0x20,
    MMC5603_REG_ST_X = 0x27,
    MMC5603_REG_ST_Y = 0x28,
    MMC5603_REG_ST_Z = 0x29,
    MMC5603_REG_PRODUCT_ID = 0x39,
};

struct Xyz8bit {
    int8_t x;
    int8_t y;
    int8_t z;
};

class Mmc5603 {
public:
    Mmc5603(int i2cNum) : i2cNum_(i2cNum) {}

    auto readProductId(uint8_t *productIdOut) -> esp_err_t;

    auto waitForMeasurement() -> bool;

    auto readX() -> float;
    auto readX20Bit() -> float;

    // Only read the most significant byte on each axis
    auto readXYZ8Bit(Xyz8bit &out) -> esp_err_t;
    // Only read the most significant byte on X and Z axes. Takes ~800us to run.
    auto readXZ8Bit(Xyz8bit &out) -> esp_err_t;

    // Read the temperature in degrees Celsius
    auto readTemperature(int &temp) -> esp_err_t;

private:
    bool configured_ = false;
    int i2cNum_;

    void configureIfNeeded();

    auto registerReadByte(mmc5603_reg_t regAddr, uint8_t *data) const -> esp_err_t;

    [[nodiscard]] auto registerWriteByte(mmc5603_reg_t regAddr, uint8_t data) const -> esp_err_t;
};
