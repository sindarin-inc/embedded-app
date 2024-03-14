#pragma once
#if USE_LILYGO_KEYBOARD
#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

//! The board peripheral power control pin needs to be set to HIGH when using the peripheral
#define TDECK_PERI_POWERON 10

#define TDECK_SPI_MOSI (gpio_num_t)41
#define TDECK_SPI_MISO (gpio_num_t)38
#define TDECK_SPI_SCK (gpio_num_t)40

#define TDECK_SDCARD_CS 39

#define TDECK_TFT_CS (gpio_num_t)12
#define TDECK_TFT_DC (gpio_num_t)11
#define TDECK_TFT_BACKLIGHT (gpio_num_t)42

#define TDECK_RADIO_CS 9
#define TDECK_RADIO_BUSY 13
#define TDECK_RADIO_RST 17
#define TDECK_RADIO_DIO1 45

#define TDECK_I2C_SDA 18
#define TDECK_I2C_SCL 8

#define TDECK_TOUCH_INT 16
#define TDECK_KEYBOARD_INT 46

#define TDECK_KEYBOARD_ADDR 0x55

#define TDECK_TRACKBALL_UP 3
#define TDECK_TRACKBALL_DOWN 15
#define TDECK_TRACKBALL_LEFT 1
#define TDECK_TRACKBALL_RIGHT 2
#define TDECK_TRACKBALL_CLICK 0

#define TDECK_ES7210_MCLK 48
#define TDECK_ES7210_LRCK 21
#define TDECK_ES7210_SCK 47
#define TDECK_ES7210_DIN 14

#define TDECK_I2S_WS 5
#define TDECK_I2S_BCK 7
#define TDECK_I2S_DOUT 6

#define TDECK_BAT_ADC 4

#include <cstring>
#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define RED rgb565(255, 0, 0)       // 0xf800
#define GREEN rgb565(0, 255, 0)     // 0x07e0
#define BLUE rgb565(0, 0, 255)      // 0x001f
#define BLACK rgb565(0, 0, 0)       // 0x0000
#define WHITE rgb565(255, 255, 255) // 0xffff
#define GRAY rgb565(128, 128, 128)  // 0x8410
#define YELLOW rgb565(255, 255, 0)  // 0xFFE0
#define CYAN rgb565(0, 156, 209)    // 0x04FA
#define PURPLE rgb565(128, 0, 128)  // 0x8010

typedef enum { DIRECTION0, DIRECTION90, DIRECTION180, DIRECTION270 } DIRECTION;

typedef enum {
    SCROLL_RIGHT = 1,
    SCROLL_LEFT = 2,
    SCROLL_DOWN = 3,
    SCROLL_UP = 4,
} SCROLL_TYPE_t;

typedef struct {
    uint16_t _width;
    uint16_t _height;
    uint16_t _offsetx;
    uint16_t _offsety;
    uint16_t _font_direction;
    uint16_t _font_fill;
    uint16_t _font_fill_color;
    uint16_t _font_underline;
    uint16_t _font_underline_color;
    gpio_num_t _dc;
    gpio_num_t _bl;
    spi_device_handle_t _SPIHandle;
    bool _use_frame_buffer;
    uint16_t *_frame_buffer;
} TFT_t;

void spi_master_init(TFT_t *dev, gpio_num_t GPIO_MOSI, gpio_num_t GPIO_SCLK, gpio_num_t GPIO_CS,
                     gpio_num_t GPIO_DC, gpio_num_t GPIO_RESET, gpio_num_t GPIO_BL);
bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t *Data, size_t DataLength);
bool spi_master_write_command(TFT_t *dev, uint8_t cmd);
bool spi_master_write_data_byte(TFT_t *dev, uint8_t data);
bool spi_master_write_data_word(TFT_t *dev, uint16_t data);
bool spi_master_write_addr(TFT_t *dev, uint16_t addr1, uint16_t addr2);
bool spi_master_write_color(TFT_t *dev, uint16_t color, uint16_t size);
bool spi_master_write_colors(TFT_t *dev, uint16_t *colors, uint16_t size);

void delayMS(int ms);
void lcdInit(TFT_t *dev, int width, int height, int offsetx, int offsety);
void lcdDrawPixel(TFT_t *dev, uint16_t x, uint16_t y, uint16_t color);
void lcdDrawMultiPixels(TFT_t *dev, uint16_t x, uint16_t y, uint16_t size, uint16_t *colors);
void lcdDrawFillRect(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                     uint16_t color);
void lcdDisplayOff(TFT_t *dev);
void lcdDisplayOn(TFT_t *dev);
void lcdFillScreen(TFT_t *dev, uint16_t color);
void lcdDrawLine(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcdDrawRect(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcdDrawRectAngle(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle,
                      uint16_t color);
void lcdDrawTriangle(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle,
                     uint16_t color);
void lcdDrawRegularPolygon(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t n, uint16_t r,
                           uint16_t angle, uint16_t color);
void lcdDrawCircle(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void lcdDrawFillCircle(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void lcdDrawRoundRect(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r,
                      uint16_t color);
void lcdDrawArrow(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t w,
                  uint16_t color);
void lcdDrawFillArrow(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t w,
                      uint16_t color);
void lcdBacklightOff(TFT_t *dev);
void lcdBacklightOn(TFT_t *dev);
void lcdInversionOff(TFT_t *dev);
void lcdInversionOn(TFT_t *dev);
void lcdWrapArround(TFT_t *dev, SCROLL_TYPE_t scroll, int start, int end);
void lcdDrawFinish(TFT_t *dev);

#define LCD_CMD 0
#define LCD_DATA 1

class DisplayLilygo : public DisplaySystem {

public:
    DisplayLilygo(gpio_num_t dc, gpio_num_t cs, gpio_num_t sck, gpio_num_t mosi, gpio_num_t miso)
        : pinDC(dc), pinCS(cs), pinSCK(sck), pinMOSI(mosi), pinMISO(miso), gfxCanvas_(320, 240) {
        gfxClass_ = &gfxCanvas_;
        canvasBuffer_ = gfxCanvas_.getBuffer();
        canvasWidth_ = 320;
        canvasHeight_ = 240;
    }

    DisplayLilygo(DisplayLilygo const &) = delete;

    ~DisplayLilygo() override = default;

    void init() {
        // Initialize non-SPI GPIOs
        // gpio_set_direction((gpio_num_t)pinDC, GPIO_MODE_OUTPUT);
        // Initialize SPI
        initSPI();
        // More initialization commands for the ST7789 go here
        initST7789();
        // lcdFillScreen(&dev, RED);
        // lcdDrawFinish(&dev);
    }

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

    void clearFrameBuffer() override { lcdFillScreen(&dev, BLACK); };

private:
    void initST7789() { lcdInit(&dev, 240, 320, 0, 0); }

    void initSPI() {
        spi_master_init(&dev, pinMOSI, pinSCK, pinCS, pinDC, (gpio_num_t)17,
                        (gpio_num_t)TDECK_TFT_BACKLIGHT);
    }

    TFT_t dev;

    gpio_num_t pinDC, pinCS, pinSCK, pinMOSI, pinMISO;
    GFXcanvas1 gfxCanvas_;
    int color_ = 0;
};

// Usage example:
// DisplayLilygo display(TDECK_TFT_DC, TDECK_TFT_CS, TDECK_SPI_SCK, TDECK_SPI_MOSI,
// TDECK_SPI_MISO); display.init();
#endif