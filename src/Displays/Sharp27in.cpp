#if CONFIG_DISPLAY_SHARP_27IN

#include "Sharp27in.hpp"

#include <algorithm>
#include <cstring>
#include <esp_err.h>
#include <math.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

/**************************************************************************
    Sharp Memory Display Connector
    -----------------------------------------------------------------------
    Pin   Function        Notes
    ===   ==============  ===============================
      1   VIN             3.3-5.0V (into LDO supply)
      2   3V3             3.3V out
      3   GND
      4   SCLK            Serial Clock
      5   MOSI            Serial Data Input
      6   CS              Serial Chip Select
      9   EXTMODE         COM Inversion Select (Low = SW clock/serial)
      7   EXTCOMIN        External COM Inversion Signal
      8   DISP            Display On(High)/Off(Low)

 **************************************************************************/

#define TOGGLE_VCOM                                                                                \
    do {                                                                                           \
        sharpmemVcom_ = sharpmemVcom_ ? 0x00 : SHARPMEM_BIT_VCOM;                                  \
    } while (0);

uint8_t inline reverse8Bits(uint8_t value) {
    uint8_t reversed = 0;
    for (int i = 0; i < 8; ++i) {
        if (value & (1 << i)) {
            reversed |= (1 << (7 - i));
        }
    }
    return reversed;
}

/**
 * @brief Construct a new Sharp27in object
 *
 * @param clk The clock pin
 * @param mosi The MOSI pin
 * @param cs The display chip select pin - **NOTE** this is ACTIVE HIGH!
 */
Sharp27in::Sharp27in(uint8_t clk, uint8_t mosi, uint8_t cs) {
    cs_ = cs;
    mosi_ = mosi;
    clk_ = clk;
}

/**
 * @brief Start the driver object, setting up pins and configuring a buffer for
 * the screen contents
 *
 * @return boolean true: success false: failure
 */
bool Sharp27in::begin(void) {
    PinMode(cs_, OUTPUT);

    // this display is weird in that cs_ is active HIGH not LOW like every other
    // SPI device
    DigitalWrite(cs_, LOW);

    spi_bus_config_t buscfg = {
        .mosi_io_num = CONFIG_DISPLAY_SHARP_27IN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = CONFIG_DISPLAY_SHARP_27IN_CLK,
        // These must be unset or the SPI driver will try to use them
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPI_DEVICE_TXBIT_LSBFIRST,
    };

    esp_err_t err = spi_bus_initialize(DISPLAY_SPI, &buscfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        log_e("Error initializing SPI bus %d", err);
        return false;
    }

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .mode = 0,

        .clock_source = SPI_CLK_SRC_DEFAULT,
        .clock_speed_hz = 2 * 1000 * 1000,

        .spics_io_num = -1,
        .queue_size = 1,
    };

    err = spi_bus_add_device(DISPLAY_SPI, &devcfg, &spiHigh_);
    if (err != ESP_OK) {
        log_e("Error adding display device to SPI bus: %d", err);
        return false;
    }

    // Set the vcom bit to a defined state
    sharpmemVcom_ = SHARPMEM_BIT_VCOM;

    clearDisplay();

    return true;
}

void inline Sharp27in::writeSPI(const uint8_t *data, size_t len) {
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    esp_err_t err = spi_device_polling_transmit(spiHigh_, &t);

    if (err != ESP_OK) {
        log_e("Error writing to SPI of length %d", len);
    }
}

/**************************************************************************/
/*!
    @brief Clears the screen
*/
/**************************************************************************/
void Sharp27in::clearDisplay() {
    // Send the clear screen command rather than doing a HW refresh (quicker)
    DigitalWrite(cs_, HIGH);

    uint8_t clear_data[2] = {reverse8Bits((uint8_t)(sharpmemVcom_ | SHARPMEM_BIT_CLEAR)), 0x00};

    writeSPI(clear_data, 2);

    TOGGLE_VCOM;
    DigitalWrite(cs_, LOW);
}

/**************************************************************************/
/*!
    @brief Renders the contents of the pixel buffer on the LCD
*/
/**************************************************************************/
void Sharp27in::refresh(uint8_t *sharpmemBuffer) {
    uint16_t i, currentline;

    DigitalWrite(cs_, HIGH);

    // Send the write command
    uint8_t command = sharpmemVcom_ | SHARPMEM_BIT_WRITECMD;
    command = reverse8Bits(command);
    writeSPI(&command, 1);

    TOGGLE_VCOM;

    uint8_t bytes_per_line = SHARP_27IN_X_PIXELS / 8;
    uint16_t totalbytes = (SHARP_27IN_X_PIXELS * SHARP_27IN_Y_PIXELS) / 8;

    for (i = 0; i < totalbytes; i += bytes_per_line) {
        uint8_t line[bytes_per_line + 2];
        // Send address byte
        currentline = ((i + 1) / (SHARP_27IN_X_PIXELS / 8)) + 1;
        line[0] = reverse8Bits(currentline);

        // copy over this line
        // Interestnigly, all the commands have to have their bits reversed
        // but our canvas already has the bits the way this display wants them
        memcpy(line + 1, sharpmemBuffer + i, bytes_per_line);

        // Send end of line
        line[bytes_per_line + 1] = 0x00;

        // send it!
        writeSPI(line, bytes_per_line + 2);
    }

    // Send another trailing 8 bits for the last line
    uint8_t zero = 0x00;
    writeSPI(&zero, 1);
    DigitalWrite(cs_, LOW);
}

#endif