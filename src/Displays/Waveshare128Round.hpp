#pragma once

#include <array>
#include <cstdint>
#include <driver/spi_master.h>
#include <stdbool.h>
#include <string>

#include "EinkTypes.hpp"
#include "Misc/Dispatch.hpp"

#define UWORD uint16_t
#define UDOUBLE uint32_t
#define UBYTE uint8_t

#define WAVESHARE_128ROUND_X_PIXELS 240
#define WAVESHARE_128ROUND_Y_PIXELS 240

#define HIGH 1
#define LOW 0

#define LCD_1IN28_HEIGHT WAVESHARE_128ROUND_Y_PIXELS
#define LCD_1IN28_WIDTH WAVESHARE_128ROUND_X_PIXELS

typedef struct{
	UWORD WIDTH;
	UWORD HEIGHT;
	UBYTE SCAN_DIR;
}LCD_1IN28_ATTRIBUTES;


/**
 * @brief Class to control a Sharp memory display
 *
 */
class Waveshare128Round {
public:
    Waveshare128Round(uint8_t clk, uint8_t mosi, uint8_t cs);
    bool begin();
    void inline writeSPI(const uint8_t *data, size_t len);

    void Init(UBYTE Scan_dir);
    void Clear(UWORD Color);
    void Display(UBYTE *Image);
    void DisplayWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD *Image);
    void DisplayPoint(UWORD X, UWORD Y, UWORD Color);

private:

    void Reset(void);
    void SendCommand(UBYTE Reg);
    void SendData_8Bit(UBYTE Data);
    void SendData_16Bit(UWORD Data);
    void InitReg(void);
    void SetAttributes(UBYTE Scan_dir);
    void SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend);


    spi_device_handle_t spiHigh_;
    DispatchQueue *einkQueue_;

    uint8_t cs_;
    uint8_t mosi_;
    uint8_t clk_;
    uint8_t rst_;

    uint8_t *sharpmemBuffer_ = NULL;
    uint8_t sharpmemVcom_;
};