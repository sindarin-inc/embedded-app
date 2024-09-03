#pragma once

#include <array>
#include <cstdint>
#include <driver/spi_master.h>
#include <stdbool.h>
#include <string>

#include "EinkTypes.hpp"
#include "Misc/Dispatch.hpp"

#define SHARP_27IN_X_PIXELS 400
#define SHARP_27IN_Y_PIXELS 240

#define HIGH 1
#define LOW 0

#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 in LSB format
#define SHARPMEM_BIT_VCOM (0x02)     // 0x40 in LSB format
#define SHARPMEM_BIT_CLEAR (0x04)    // 0x20 in LSB format

/**
 * @brief Class to control a Sharp memory display
 *
 */
class Sharp27in {
public:
    Sharp27in(uint8_t clk, uint8_t mosi, uint8_t cs);
    bool begin();
    void clearDisplay();
    void refresh(uint8_t *sharpmemBuffer);
    void inline writeSPI(const uint8_t *data, size_t len);

private:
    spi_device_handle_t spiHigh_;
    DispatchQueue *einkQueue_;

    uint8_t cs_;
    uint8_t mosi_;
    uint8_t clk_;
    uint8_t rst_;

    uint8_t *sharpmemBuffer_ = NULL;
    uint8_t sharpmemVcom_;
};