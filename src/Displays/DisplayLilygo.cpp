#if USE_LILYGO_KEYBOARD
#include "DisplayLilygo.hpp"

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "ST7789"
#define _DEBUG_ 0

#define CONFIG_FRAME_BUFFER 1

#if 0
#ifdef CONFIG_IDF_TARGET_ESP32
#define LCD_HOST HSPI_HOST
#elif defined CONFIG_IDF_TARGET_ESP32S2
#define LCD_HOST SPI2_HOST
#elif defined CONFIG_IDF_TARGET_ESP32S3
#define LCD_HOST SPI2_HOST
#elif defined CONFIG_IDF_TARGET_ESP32C3
#define LCD_HOST SPI2_HOST
#endif
#endif

#if CONFIG_SPI2_HOST
#define HOST_ID SPI2_HOST
#elif CONFIG_SPI3_HOST
#define HOST_ID SPI3_HOST
#endif

static const int SPI_Command_Mode = 0;
static const int SPI_Data_Mode = 1;
static const int SPI_Frequency = SPI_MASTER_FREQ_20M;
// static const int SPI_Frequency = SPI_MASTER_FREQ_26M;
// static const int SPI_Frequency = SPI_MASTER_FREQ_40M;
// static const int SPI_Frequency = SPI_MASTER_FREQ_80M;

void spi_master_init(TFT_t *dev, gpio_num_t GPIO_MOSI, gpio_num_t GPIO_SCLK, gpio_num_t GPIO_CS,
                     gpio_num_t GPIO_DC, gpio_num_t GPIO_RESET, gpio_num_t GPIO_BL) {
    esp_err_t ret;

    ESP_LOGI(TAG, "GPIO_CS=%d", GPIO_CS);
    if (GPIO_CS >= 0) {
        // gpio_pad_select_gpio( GPIO_CS );
        gpio_reset_pin(GPIO_CS);
        gpio_set_direction(GPIO_CS, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_CS, 0);
    }

    ESP_LOGI(TAG, "GPIO_DC=%d", GPIO_DC);
    // gpio_pad_select_gpio( GPIO_DC );
    gpio_reset_pin(GPIO_DC);
    gpio_set_direction(GPIO_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_DC, 0);

    ESP_LOGI(TAG, "GPIO_RESET=%d", GPIO_RESET);
    if (GPIO_RESET >= 0) {
        // gpio_pad_select_gpio( GPIO_RESET );
        gpio_reset_pin(GPIO_RESET);
        gpio_set_direction(GPIO_RESET, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_RESET, 1);
        delayMS(100);
        gpio_set_level(GPIO_RESET, 0);
        delayMS(100);
        gpio_set_level(GPIO_RESET, 1);
        delayMS(100);
    }

    ESP_LOGI(TAG, "GPIO_BL=%d", GPIO_BL);
    if (GPIO_BL >= 0) {
        // gpio_pad_select_gpio(GPIO_BL);
        gpio_reset_pin(GPIO_BL);
        gpio_set_direction(GPIO_BL, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_BL, 0);
    }

    ESP_LOGI(TAG, "GPIO_MOSI=%d", GPIO_MOSI);
    ESP_LOGI(TAG, "GPIO_SCLK=%d", GPIO_SCLK);
    spi_bus_config_t buscfg = {.mosi_io_num = GPIO_MOSI,
                               .miso_io_num = -1,
                               .sclk_io_num = GPIO_SCLK,
                               .quadwp_io_num = -1,
                               .quadhd_io_num = -1,
                               .max_transfer_sz = 0,
                               .flags = 0};

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_LOGD(TAG, "spi_bus_initialize=%d", ret);
    assert(ret == ESP_OK);

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz = SPI_Frequency;
    devcfg.queue_size = 7;
    // devcfg.mode = 2;
    devcfg.mode = 3;
    devcfg.flags = SPI_DEVICE_NO_DUMMY;

    if (GPIO_CS >= 0) {
        devcfg.spics_io_num = GPIO_CS;
    } else {
        devcfg.spics_io_num = -1;
    }

    spi_device_handle_t handle;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    ESP_LOGD(TAG, "spi_bus_add_device=%d", ret);
    assert(ret == ESP_OK);
    dev->_dc = GPIO_DC;
    dev->_bl = GPIO_BL;
    dev->_SPIHandle = handle;
}

bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t *Data, size_t DataLength) {
    spi_transaction_t SPITransaction;
    esp_err_t ret;

    if (DataLength > 0) {
        memset(&SPITransaction, 0, sizeof(spi_transaction_t));
        SPITransaction.length = DataLength * 8;
        SPITransaction.tx_buffer = Data;
#if 1
        ret = spi_device_transmit(SPIHandle, &SPITransaction);
#else
        ret = spi_device_polling_transmit(SPIHandle, &SPITransaction);
#endif
        assert(ret == ESP_OK);
    }

    return true;
}

bool spi_master_write_command(TFT_t *dev, uint8_t cmd) {
    static uint8_t Byte = 0;
    Byte = cmd;
    gpio_set_level(dev->_dc, SPI_Command_Mode);
    return spi_master_write_byte(dev->_SPIHandle, &Byte, 1);
}

bool spi_master_write_data_byte(TFT_t *dev, uint8_t data) {
    static uint8_t Byte = 0;
    Byte = data;
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, &Byte, 1);
}

bool spi_master_write_data_word(TFT_t *dev, uint16_t data) {
    static uint8_t Byte[2];
    Byte[0] = (data >> 8) & 0xFF;
    Byte[1] = data & 0xFF;
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, 2);
}

bool spi_master_write_addr(TFT_t *dev, uint16_t addr1, uint16_t addr2) {
    static uint8_t Byte[4];
    Byte[0] = (addr1 >> 8) & 0xFF;
    Byte[1] = addr1 & 0xFF;
    Byte[2] = (addr2 >> 8) & 0xFF;
    Byte[3] = addr2 & 0xFF;
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, 4);
}

bool spi_master_write_color(TFT_t *dev, uint16_t color, uint16_t size) {
    static uint8_t Byte[1024];
    int index = 0;
    for (int i = 0; i < size; i++) {
        Byte[index++] = (color >> 8) & 0xFF;
        Byte[index++] = color & 0xFF;
    }
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, size * 2);
}

// Add 202001
bool spi_master_write_colors(TFT_t *dev, uint16_t *colors, uint16_t size) {
    static uint8_t Byte[1024];
    int index = 0;
    for (int i = 0; i < size; i++) {
        Byte[index++] = (colors[i] >> 8) & 0xFF;
        Byte[index++] = colors[i] & 0xFF;
    }
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, size * 2);
}

void delayMS(int ms) {
    int _ms = ms + (portTICK_PERIOD_MS - 1);
    TickType_t xTicksToDelay = _ms / portTICK_PERIOD_MS;
    ESP_LOGD(TAG, "ms=%d _ms=%d portTICK_PERIOD_MS=%" PRIu32 " xTicksToDelay=%" PRIu32, ms, _ms,
             portTICK_PERIOD_MS, xTicksToDelay);
    vTaskDelay(xTicksToDelay);
}

void lcdInit(TFT_t *dev, int width, int height, int offsetx, int offsety) {
    dev->_width = width;
    dev->_height = height;
    dev->_offsetx = offsetx;
    dev->_offsety = offsety;
    dev->_font_direction = DIRECTION0;
    dev->_font_fill = false;
    dev->_font_underline = false;

    spi_master_write_command(dev, 0x01); // Software Reset
    delayMS(150);

    spi_master_write_command(dev, 0x11); // Sleep Out
    delayMS(255);

    spi_master_write_command(dev, 0x3A); // Interface Pixel Format
    spi_master_write_data_byte(dev, 0x55);
    delayMS(10);

    spi_master_write_command(dev, 0x36); // Memory Data Access Control
    spi_master_write_data_byte(dev, 0x00);

    spi_master_write_command(dev, 0x2A); // Column Address Set
    spi_master_write_data_byte(dev, 0x00);
    spi_master_write_data_byte(dev, 0x00);
    spi_master_write_data_byte(dev, 0x00);
    spi_master_write_data_byte(dev, 0xF0);

    spi_master_write_command(dev, 0x2B); // Row Address Set
    spi_master_write_data_byte(dev, 0x00);
    spi_master_write_data_byte(dev, 0x00);
    spi_master_write_data_byte(dev, 0x00);
    spi_master_write_data_byte(dev, 0xF0);

    spi_master_write_command(dev, 0x21); // Display Inversion On
    delayMS(10);

    spi_master_write_command(dev, 0x13); // Normal Display Mode On
    delayMS(10);

    spi_master_write_command(dev, 0x29); // Display ON
    delayMS(255);

    if (dev->_bl >= 0) {
        gpio_set_level(dev->_bl, 1);
    }

    dev->_use_frame_buffer = false;
#if CONFIG_FRAME_BUFFER
    dev->_frame_buffer =
        (uint16_t *)heap_caps_malloc(sizeof(uint16_t) * width * height, MALLOC_CAP_SPIRAM);
    if (dev->_frame_buffer == NULL) {
        ESP_LOGE(TAG, "heap_caps_malloc fail");
    } else {
        ESP_LOGI(TAG, "heap_caps_malloc success");
        dev->_use_frame_buffer = true;
    }

#endif
}

// Draw pixel
// x:X coordinate
// y:Y coordinate
// color:color
void lcdDrawPixel(TFT_t *dev, uint16_t x, uint16_t y, uint16_t color) {
    if (x >= dev->_width) return;
    if (y >= dev->_height) return;

    if (dev->_use_frame_buffer) {
        dev->_frame_buffer[y * dev->_width + x] = color;
    } else {
        uint16_t _x = x + dev->_offsetx;
        uint16_t _y = y + dev->_offsety;

        spi_master_write_command(dev, 0x2A); // set column(x) address
        spi_master_write_addr(dev, _x, _x);
        spi_master_write_command(dev, 0x2B); // set Page(y) address
        spi_master_write_addr(dev, _y, _y);
        spi_master_write_command(dev, 0x2C); // Memory Write
        // spi_master_write_data_word(dev, color);
        spi_master_write_colors(dev, &color, 1);
    }
}

// Draw multi pixel
// x:X coordinate
// y:Y coordinate
// size:Number of colors
// colors:colors
void lcdDrawMultiPixels(TFT_t *dev, uint16_t x, uint16_t y, uint16_t size, uint16_t *colors) {
    if (x + size > dev->_width) return;
    if (y >= dev->_height) return;

    if (dev->_use_frame_buffer) {
        uint16_t _x1 = x;
        uint16_t _x2 = _x1 + (size - 1);
        uint16_t _y1 = y;
        uint16_t _y2 = _y1;
        int16_t index = 0;
        for (int16_t j = _y1; j <= _y2; j++) {
            for (int16_t i = _x1; i <= _x2; i++) {
                dev->_frame_buffer[j * dev->_width + i] = colors[index++];
            }
        }
    } else {
        uint16_t _x1 = x + dev->_offsetx;
        uint16_t _x2 = _x1 + (size - 1);
        uint16_t _y1 = y + dev->_offsety;
        uint16_t _y2 = _y1;

        spi_master_write_command(dev, 0x2A); // set column(x) address
        spi_master_write_addr(dev, _x1, _x2);
        spi_master_write_command(dev, 0x2B); // set Page(y) address
        spi_master_write_addr(dev, _y1, _y2);
        spi_master_write_command(dev, 0x2C); // Memory Write
        spi_master_write_colors(dev, colors, size);
    }
}

// Draw rectangle of filling
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End X coordinate
// y2:End Y coordinate
// color:color
void lcdDrawFillRect(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                     uint16_t color) {
    if (x1 >= dev->_width) return;
    if (x2 >= dev->_width) x2 = dev->_width - 1;
    if (y1 >= dev->_height) return;
    if (y2 >= dev->_height) y2 = dev->_height - 1;

    ESP_LOGD(TAG, "offset(x)=%d offset(y)=%d", dev->_offsetx, dev->_offsety);

    if (dev->_use_frame_buffer) {
        for (int16_t j = y1; j <= y2; j++) {
            for (int16_t i = x1; i <= x2; i++) {
                dev->_frame_buffer[j * dev->_width + i] = color;
            }
        }
    } else {
        uint16_t _x1 = x1 + dev->_offsetx;
        uint16_t _x2 = x2 + dev->_offsetx;
        uint16_t _y1 = y1 + dev->_offsety;
        uint16_t _y2 = y2 + dev->_offsety;

        spi_master_write_command(dev, 0x2A); // set column(x) address
        spi_master_write_addr(dev, _x1, _x2);
        spi_master_write_command(dev, 0x2B); // set Page(y) address
        spi_master_write_addr(dev, _y1, _y2);
        spi_master_write_command(dev, 0x2C); // Memory Write
        for (int i = _x1; i <= _x2; i++) {
            uint16_t size = _y2 - _y1 + 1;
            spi_master_write_color(dev, color, size);
        }
    }
}

// Display OFF
void lcdDisplayOff(TFT_t *dev) {
    spi_master_write_command(dev, 0x28); // Display off
}

// Display ON
void lcdDisplayOn(TFT_t *dev) {
    spi_master_write_command(dev, 0x29); // Display on
}

// Fill screen
// color:color
void lcdFillScreen(TFT_t *dev, uint16_t color) {
    lcdDrawFillRect(dev, 0, 0, dev->_width - 1, dev->_height - 1, color);
}

// Draw line
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// color:color
void lcdDrawLine(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    int i;
    int dx, dy;
    int sx, sy;
    int E;

    /* distance between two points */
    dx = (x2 > x1) ? x2 - x1 : x1 - x2;
    dy = (y2 > y1) ? y2 - y1 : y1 - y2;

    /* direction of two point */
    sx = (x2 > x1) ? 1 : -1;
    sy = (y2 > y1) ? 1 : -1;

    /* inclination < 1 */
    if (dx > dy) {
        E = -dx;
        for (i = 0; i <= dx; i++) {
            lcdDrawPixel(dev, x1, y1, color);
            x1 += sx;
            E += 2 * dy;
            if (E >= 0) {
                y1 += sy;
                E -= 2 * dx;
            }
        }

        /* inclination >= 1 */
    } else {
        E = -dy;
        for (i = 0; i <= dy; i++) {
            lcdDrawPixel(dev, x1, y1, color);
            y1 += sy;
            E += 2 * dx;
            if (E >= 0) {
                x1 += sx;
                E -= 2 * dy;
            }
        }
    }
}

// Draw rectangle
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// color:color
void lcdDrawRect(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    lcdDrawLine(dev, x1, y1, x2, y1, color);
    lcdDrawLine(dev, x2, y1, x2, y2, color);
    lcdDrawLine(dev, x2, y2, x1, y2, color);
    lcdDrawLine(dev, x1, y2, x1, y1, color);
}

// Draw rectangle with angle
// xc:Center X coordinate
// yc:Center Y coordinate
// w:Width of rectangle
// h:Height of rectangle
// angle:Angle of rectangle
// color:color

// When the origin is (0, 0), the point (x1, y1) after rotating the point (x, y) by the angle is
// obtained by the following calculation.
//  x1 = x * cos(angle) - y * sin(angle)
//  y1 = x * sin(angle) + y * cos(angle)
void lcdDrawRectAngle(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle,
                      uint16_t color) {
    double xd, yd, rd;
    int x1, y1;
    int x2, y2;
    int x3, y3;
    int x4, y4;
    rd = -angle * M_PI / 180.0;
    xd = 0.0 - w / 2;
    yd = h / 2;
    x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    yd = 0.0 - yd;
    x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    xd = w / 2;
    yd = h / 2;
    x3 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y3 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    yd = 0.0 - yd;
    x4 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y4 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    lcdDrawLine(dev, x1, y1, x2, y2, color);
    lcdDrawLine(dev, x1, y1, x3, y3, color);
    lcdDrawLine(dev, x2, y2, x4, y4, color);
    lcdDrawLine(dev, x3, y3, x4, y4, color);
}

// Draw triangle
// xc:Center X coordinate
// yc:Center Y coordinate
// w:Width of triangle
// h:Height of triangle
// angle:Angle of triangle
// color:color

// When the origin is (0, 0), the point (x1, y1) after rotating the point (x, y) by the angle is
// obtained by the following calculation.
//  x1 = x * cos(angle) - y * sin(angle)
//  y1 = x * sin(angle) + y * cos(angle)
void lcdDrawTriangle(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle,
                     uint16_t color) {
    double xd, yd, rd;
    int x1, y1;
    int x2, y2;
    int x3, y3;
    rd = -angle * M_PI / 180.0;
    xd = 0.0;
    yd = h / 2;
    x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    xd = w / 2;
    yd = 0.0 - yd;
    x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    xd = 0.0 - w / 2;
    x3 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
    y3 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

    lcdDrawLine(dev, x1, y1, x2, y2, color);
    lcdDrawLine(dev, x1, y1, x3, y3, color);
    lcdDrawLine(dev, x2, y2, x3, y3, color);
}

// Draw regular polygon
// xc:Center X coordinate
// yc:Center Y coordinate
// n:Number of slides
// r:radius
// angle:Angle of regular polygon
// color:color
void lcdDrawRegularPolygon(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t n, uint16_t r,
                           uint16_t angle, uint16_t color) {
    double xd, yd, rd;
    int x1, y1;
    int x2, y2;
    int i;

    rd = -angle * M_PI / 180.0;
    for (i = 0; i < n; i++) {
        xd = r * cos(2 * M_PI * i / n);
        yd = r * sin(2 * M_PI * i / n);
        x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
        y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

        xd = r * cos(2 * M_PI * (i + 1) / n);
        yd = r * sin(2 * M_PI * (i + 1) / n);
        x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
        y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

        lcdDrawLine(dev, x1, y1, x2, y2, color);
    }
}

// Draw circle
// x0:Central X coordinate
// y0:Central Y coordinate
// r:radius
// color:color
void lcdDrawCircle(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int x;
    int y;
    int err;
    int old_err;

    x = 0;
    y = -r;
    err = 2 - 2 * r;
    do {
        lcdDrawPixel(dev, x0 - x, y0 + y, color);
        lcdDrawPixel(dev, x0 - y, y0 - x, color);
        lcdDrawPixel(dev, x0 + x, y0 - y, color);
        lcdDrawPixel(dev, x0 + y, y0 + x, color);
        if ((old_err = err) <= x) err += ++x * 2 + 1;
        if (old_err > y || err > x) err += ++y * 2 + 1;
    } while (y < 0);
}

// Draw circle of filling
// x0:Central X coordinate
// y0:Central Y coordinate
// r:radius
// color:color
void lcdDrawFillCircle(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int x;
    int y;
    int err;
    int old_err;
    int ChangeX;

    x = 0;
    y = -r;
    err = 2 - 2 * r;
    ChangeX = 1;
    do {
        if (ChangeX) {
            lcdDrawLine(dev, x0 - x, y0 - y, x0 - x, y0 + y, color);
            lcdDrawLine(dev, x0 + x, y0 - y, x0 + x, y0 + y, color);
        } // endif
        ChangeX = (old_err = err) <= x;
        if (ChangeX) err += ++x * 2 + 1;
        if (old_err > y || err > x) err += ++y * 2 + 1;
    } while (y <= 0);
}

// Draw rectangle with round corner
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// r:radius
// color:color
void lcdDrawRoundRect(TFT_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r,
                      uint16_t color) {
    int x;
    int y;
    int err;
    int old_err;
    unsigned char temp;

    if (x1 > x2) {
        temp = x1;
        x1 = x2;
        x2 = temp;
    } // endif

    if (y1 > y2) {
        temp = y1;
        y1 = y2;
        y2 = temp;
    } // endif

    ESP_LOGD(TAG, "x1=%d x2=%d delta=%d r=%d", x1, x2, x2 - x1, r);
    ESP_LOGD(TAG, "y1=%d y2=%d delta=%d r=%d", y1, y2, y2 - y1, r);
    if (x2 - x1 < r) return; // Add 20190517
    if (y2 - y1 < r) return; // Add 20190517

    x = 0;
    y = -r;
    err = 2 - 2 * r;

    do {
        if (x) {
            lcdDrawPixel(dev, x1 + r - x, y1 + r + y, color);
            lcdDrawPixel(dev, x2 - r + x, y1 + r + y, color);
            lcdDrawPixel(dev, x1 + r - x, y2 - r - y, color);
            lcdDrawPixel(dev, x2 - r + x, y2 - r - y, color);
        } // endif
        if ((old_err = err) <= x) err += ++x * 2 + 1;
        if (old_err > y || err > x) err += ++y * 2 + 1;
    } while (y < 0);

    ESP_LOGD(TAG, "x1+r=%d x2-r=%d", x1 + r, x2 - r);
    lcdDrawLine(dev, x1 + r, y1, x2 - r, y1, color);
    lcdDrawLine(dev, x1 + r, y2, x2 - r, y2, color);
    ESP_LOGD(TAG, "y1+r=%d y2-r=%d", y1 + r, y2 - r);
    lcdDrawLine(dev, x1, y1 + r, x1, y2 - r, color);
    lcdDrawLine(dev, x2, y1 + r, x2, y2 - r, color);
}

// Draw arrow
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// w:Width of the botom
// color:color
// Thanks http://k-hiura.cocolog-nifty.com/blog/2010/11/post-2a62.html
void lcdDrawArrow(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t w,
                  uint16_t color) {
    double Vx = x1 - x0;
    double Vy = y1 - y0;
    double v = sqrt(Vx * Vx + Vy * Vy);
    //	 printf("v=%f\n",v);
    double Ux = Vx / v;
    double Uy = Vy / v;

    uint16_t L[2], R[2];
    L[0] = x1 - Uy * w - Ux * v;
    L[1] = y1 + Ux * w - Uy * v;
    R[0] = x1 + Uy * w - Ux * v;
    R[1] = y1 - Ux * w - Uy * v;
    // printf("L=%d-%d R=%d-%d\n",L[0],L[1],R[0],R[1]);

    // lcdDrawLine(x0,y0,x1,y1,color);
    lcdDrawLine(dev, x1, y1, L[0], L[1], color);
    lcdDrawLine(dev, x1, y1, R[0], R[1], color);
    lcdDrawLine(dev, L[0], L[1], R[0], R[1], color);
}

// Draw arrow of filling
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End	X coordinate
// y2:End	Y coordinate
// w:Width of the botom
// color:color
void lcdDrawFillArrow(TFT_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t w,
                      uint16_t color) {
    double Vx = x1 - x0;
    double Vy = y1 - y0;
    double v = sqrt(Vx * Vx + Vy * Vy);
    // printf("v=%f\n",v);
    double Ux = Vx / v;
    double Uy = Vy / v;

    uint16_t L[2], R[2];
    L[0] = x1 - Uy * w - Ux * v;
    L[1] = y1 + Ux * w - Uy * v;
    R[0] = x1 + Uy * w - Ux * v;
    R[1] = y1 - Ux * w - Uy * v;
    // printf("L=%d-%d R=%d-%d\n",L[0],L[1],R[0],R[1]);

    lcdDrawLine(dev, x0, y0, x1, y1, color);
    lcdDrawLine(dev, x1, y1, L[0], L[1], color);
    lcdDrawLine(dev, x1, y1, R[0], R[1], color);
    lcdDrawLine(dev, L[0], L[1], R[0], R[1], color);

    int ww;
    for (ww = w - 1; ww > 0; ww--) {
        L[0] = x1 - Uy * ww - Ux * v;
        L[1] = y1 + Ux * ww - Uy * v;
        R[0] = x1 + Uy * ww - Ux * v;
        R[1] = y1 - Ux * ww - Uy * v;
        // printf("Fill>L=%d-%d R=%d-%d\n",L[0],L[1],R[0],R[1]);
        lcdDrawLine(dev, x1, y1, L[0], L[1], color);
        lcdDrawLine(dev, x1, y1, R[0], R[1], color);
    }
}

// Backlight OFF
void lcdBacklightOff(TFT_t *dev) {
    if (dev->_bl >= 0) {
        gpio_set_level(dev->_bl, 0);
    }
}

// Backlight ON
void lcdBacklightOn(TFT_t *dev) {
    if (dev->_bl >= 0) {
        gpio_set_level(dev->_bl, 1);
    }
}

// Display Inversion Off
void lcdInversionOff(TFT_t *dev) {
    spi_master_write_command(dev, 0x20); // Display Inversion Off
}

// Display Inversion On
void lcdInversionOn(TFT_t *dev) {
    spi_master_write_command(dev, 0x21); // Display Inversion On
}

void lcdWrapArround(TFT_t *dev, SCROLL_TYPE_t scroll, int start, int end) {
    if (dev->_use_frame_buffer == false) return;

    int _width = dev->_width;
    int _height = dev->_height;
    int32_t index1;
    int32_t index2;

    if (scroll == SCROLL_RIGHT) {
        uint16_t wk[_width];
        for (int i = start; i < end; i++) {
            index1 = i * _width;
            memcpy((char *)wk, (char *)&dev->_frame_buffer[index1], _width * 2);
            index2 = index1 + _width - 1;
            dev->_frame_buffer[index1] = dev->_frame_buffer[index2];
            memcpy((char *)&dev->_frame_buffer[index1 + 1], (char *)&wk[0], (_width - 1) * 2);
        }
    } else if (scroll == SCROLL_LEFT) {
        uint16_t wk[_width];
        for (int i = start; i < end; i++) {
            index1 = i * _width;
            memcpy((char *)wk, (char *)&dev->_frame_buffer[index1], _width * 2);
            index2 = index1 + _width - 1;
            dev->_frame_buffer[index2] = dev->_frame_buffer[index1];
            memcpy((char *)&dev->_frame_buffer[index1], (char *)&wk[1], (_width - 1) * 2);
        }
    } else if (scroll == SCROLL_UP) {
        uint16_t wk;
        for (int i = start; i <= end; i++) {
            wk = dev->_frame_buffer[i];
            for (int j = 0; j < _height - 1; j++) {
                index1 = j * _width + i;
                index2 = (j + 1) * _width + i;
                dev->_frame_buffer[index1] = dev->_frame_buffer[index2];
            }
            index2 = (_height - 1) * _width + i;
            dev->_frame_buffer[index2] = wk;
        }
    } else if (scroll == SCROLL_DOWN) {
        uint16_t wk;
        for (int i = start; i <= end; i++) {
            index2 = (_height - 1) * _width + i;
            wk = dev->_frame_buffer[index2];
            for (int j = _height - 2; j >= 0; j--) {
                index1 = j * _width + i;
                index2 = (j + 1) * _width + i;
                dev->_frame_buffer[index2] = dev->_frame_buffer[index1];
            }
            dev->_frame_buffer[i] = wk;
        }
    }
}

// Draw Frame Buffer
void lcdDrawFinish(TFT_t *dev) {
    if (dev->_use_frame_buffer == false) return;

    spi_master_write_command(dev, 0x2A); // set column(x) address
    spi_master_write_addr(dev, dev->_offsetx, dev->_offsetx + dev->_width - 1);
    spi_master_write_command(dev, 0x2B); // set Page(y) address
    spi_master_write_addr(dev, dev->_offsety, dev->_offsety + dev->_height - 1);
    spi_master_write_command(dev, 0x2C); // Memory Write

    // uint16_t size = dev->_width*dev->_height;
    uint32_t size = dev->_width * dev->_height;
    uint16_t *image = dev->_frame_buffer;
    while (size > 0) {
        // 1024 bytes per time.
        uint16_t bs = (size > 1024) ? 1024 : size;
        spi_master_write_colors(dev, image, bs);
        size -= bs;
        image += bs;
    }
    return;
}

// Usage example:
// DisplayLilygo display(TDECK_TFT_DC, TDECK_TFT_CS, TDECK_SPI_SCK, TDECK_SPI_MOSI, TDECK_SPI_MISO);
// display.init();

auto DisplayLilygo::setup() -> bool {
    DisplaySystem::setup();
    log_d("Setting up Display!");
    init();
    log_d("Just finished init of display!");
    return true;
}

void DisplayLilygo::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                  int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                  Rect altRect) {

    for (int x = 0; x < 320; x++) {
        for (int y = 0; y < 240; y++) {
            bool color = gfxCanvas_.getPixel(x, y);
            uint16_t pixelX = 240 - y;
            uint16_t pixelY = x;
            lcdDrawPixel(&dev, pixelX, pixelY, color ? BLACK : WHITE);
        }
    }

    lcdDrawFinish(&dev);
    fillScreen(bg);
}
#endif