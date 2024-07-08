#include "Displays/DisplayWaveshareUART43.hpp"

#include "Communications/USBSerial.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

extern bool mirrorToSerial;

void DisplayWaveshareUART43::fillScreen(uint16_t color) { DisplaySystem::fillScreen(color); }

void DisplayWaveshareUART43::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                          int16_t x2, int16_t y2, uint16_t color) {
    DisplaySystem::fillTriangle(x0, y0, x1, y1, x2, y2, color);
    driver_.epd_draw_triangle(x0, y0, x1, y1, x2, y2);
}

void DisplayWaveshareUART43::setFont(Font *f) { DisplaySystem::setFont(f); }

void DisplayWaveshareUART43::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    DisplaySystem::fillRect(x, y, w, h, color);
    for (int i = 0; i < h; i++) {
        driver_.epd_draw_line(x, y + i, x + w, y + i);
    }
}

void DisplayWaveshareUART43::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                      uint16_t color) {
    DisplaySystem::drawLine(x0, y0, x1, y1, color);
    driver_.epd_draw_line(x0, y0, x1, y1);
}

void DisplayWaveshareUART43::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    DisplaySystem::drawRect(x, y, w, h, color);
    driver_.epd_draw_line(x, y, x + w, y);
    driver_.epd_draw_line(x + w, y, x + w, y + h);
    driver_.epd_draw_line(x + w, y + h, x, y + h);
    driver_.epd_draw_line(x, y + h, x, y);
    Delay(1);
}

void DisplayWaveshareUART43::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    DisplaySystem::drawCircle(x0, y0, r, color);
    driver_.epd_draw_circle(x0, y0, r);
}

void DisplayWaveshareUART43::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    DisplaySystem::fillCircle(x0, y0, r, color);
    for (int i = 0; i < r; i++) {
        driver_.epd_draw_circle(x0, y0, r - i);
    }
}

void DisplayWaveshareUART43::fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h,
                                           int16_t radius, uint16_t color) {
    DisplaySystem::fillRoundRect(x0, y0, w, h, radius, color);
    // ignore the corner radius for now
    fillRect(x0, y0, w, h, color);
}

void DisplayWaveshareUART43::drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h,
                                           int16_t radius, uint16_t color) {
    DisplaySystem::drawRoundRect(x0, y0, w, h, radius, color);
    drawRect(x0, y0, w, h, color);
}

void DisplayWaveshareUART43::setRotation(uint8_t r) { DisplaySystem::setRotation(r); }

void DisplayWaveshareUART43::setCursor(int16_t x, int16_t y) { DisplaySystem::setCursor(x, y); }

void DisplayWaveshareUART43::drawBox(int startX, int startY, int endX, int endY) {
    DisplaySystem::drawBox(startX, startY, endX, endY);
    driver_.epd_draw_line(startX, startY, endX, startY);
    driver_.epd_draw_line(endX, startY, endX, endY);
    driver_.epd_draw_line(endX, endY, startX, endY);
    driver_.epd_draw_line(startX, endY, startX, startY);
}

// Draw a dotted box
void DisplayWaveshareUART43::drawDottedBox(int startX, int startY, int endX, int endY, int dotSize,
                                           int gapSize) {
    DisplaySystem::drawDottedBox(startX, startY, endX, endY, dotSize, gapSize);
    // ignore dotted part for now
    drawBox(startX, startY, endX, endY);
}

// Draw a bitmap
void DisplayWaveshareUART43::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w,
                                         int16_t h, uint16_t color) {
    DisplaySystem::drawXBitmap(x, y, bitmap, w, h, color);
    // unimplemented in this driver
}

// Draw a bitmap scaled by a multiplier
void DisplayWaveshareUART43::drawScaledXBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                                               int16_t w, int16_t h, uint16_t color, int scale) {
    DisplaySystem::drawScaledXBitmap(x, y, bitmap, w, h, color, scale);
    // unimplemented in this driver
}

void DisplayWaveshareUART43::drawPBM(int16_t x, int16_t y, const std::string &fname,
                                     FileSystem &fs) {
    DisplaySystem::drawPBM(x, y, fname, fs);
    // unimplemented in this driver
}

void DisplayWaveshareUART43::drawPBM(int16_t x, int16_t y, const uint8_t *pbmBytes, int size) {
    DisplaySystem::drawPBM(x, y, pbmBytes, size);
    // unimplemented in this driver
}

void DisplayWaveshareUART43::drawPBM(int16_t x, int16_t y, RendererStream &stream) {
    DisplaySystem::drawPBM(x, y, stream);
    // unimplemented in this driver
}

// void DisplayWaveshareUART43::clearScreen() { DisplaySystem::clearScreen(); }

void DisplayWaveshareUART43::drawSingleLineOfText(const std::string &line, int x, int y,
                                                  bool inverted) {
    DisplaySystem::drawSingleLineOfText(line, x, y, inverted);
    driver_.epd_disp_string(line.c_str(), xOffset(x),
                            yOffset(y + getTextHeight() - lineSpacing_ - 3));
}

void DisplayWaveshareUART43::draw_text_demo(void) {
    char buff[] = {'G',  'B',  'K',  '3',  '2',  ':',  ' ',  0xc4,
                   0xe3, 0xba, 0xc3, 0xca, 0xc0, 0xbd, 0xe7, 0};
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_clear();
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    driver_.epd_disp_string(buff, 0, 50);
    driver_.epd_disp_string("ASCII32: Hello, World!", 0, 300);

    driver_.epd_set_ch_font(GBK48);
    driver_.epd_set_en_font(ASCII48);
    buff[3] = '4';
    buff[4] = '8';
    driver_.epd_disp_string(buff, 0, 100);
    driver_.epd_disp_string("ASCII48: Hello, World!", 0, 350);

    driver_.epd_set_ch_font(GBK64);
    driver_.epd_set_en_font(ASCII64);
    buff[3] = '6';
    buff[4] = '4';
    driver_.epd_disp_string(buff, 0, 160);
    driver_.epd_disp_string("ASCII64: Hello, World!", 0, 450);

    driver_.epd_update();
    Delay(2000);
}

auto DisplayWaveshareUART43::setup() -> bool {

    DisplaySystem::setup();
    driver_.epd_init();
    driver_.epd_wakeup();
    driver_.epd_set_memory(MEM_NAND);
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    driver_.epd_clear();
    log_d("Drawing a bunch of stuff in setup");

    int i;

    /*
    draw pixel
    */
    /*
     for (j = 0; j < 600; j += 50) {
         for (i = 0; i < 800; i += 50) {
             driver_.epd_draw_pixel(i, j);
             driver_.epd_draw_pixel(i, j + 1);
             driver_.epd_draw_pixel(i + 1, j);
             driver_.epd_draw_pixel(i + 1, j + 1);
         }
     }
     driver_.epd_update();
     Delay(3000);
 */
    /*
    draw line
    */
    /*
     driver_.epd_clear();
     for (i = 0; i < 800; i += 100) {
         driver_.epd_draw_line(0, 0, i, 599);
         driver_.epd_draw_line(799, 0, i, 599);
     }
     driver_.epd_update();
     Delay(3000);
 */
    /*
    fill rect
    */
    /*
     driver_.epd_clear();
     driver_.epd_set_color(BLACK, WHITE);
     driver_.epd_fill_rect(10, 10, 100, 100);

     driver_.epd_set_color(DARK_GRAY, WHITE);
     driver_.epd_fill_rect(110, 10, 200, 100);

     driver_.epd_set_color(GRAY, WHITE);
     driver_.epd_fill_rect(210, 10, 300, 100);

     driver_.epd_update();
     Delay(3000);
 */
    /*
    draw circle
    */
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_clear();
    for (i = 0; i < 300; i += 40) {
        driver_.epd_draw_circle(399, 299, i);
    }
    driver_.epd_update();
    Delay(3000);

    /*
    fill circle
    */
    /*
     driver_.epd_clear();
     for (j = 0; j < 6; j++) {
         for (i = 0; i < 8; i++) {
             driver_.epd_fill_circle(50 + i * 100, 50 + j * 100, 50);
         }
     }
     driver_.epd_update();
     Delay(3000);
 */
    /*
    draw triangle
    */
    /*
     driver_.epd_clear();
     for (i = 1; i < 5; i++) {
         driver_.epd_draw_triangle(399, 249 - i * 50, 349 - i * 50, 349 + i * 50, 449 + i * 50,
                                   349 + i * 50);
     }
     driver_.epd_update();
     Delay(3000);
     */
    /*
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    driver_.epd_disp_string("Sol Reader", 350, 275);
    log_d("Displaying Sol Reader");
    driver_.epd_update();
    Delay(2000);*/
    /*
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_clear();
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    log_d("Displaying Another intro text");
    driver_.epd_disp_string("Another intro text", 350, 275);
    driver_.epd_update();
    Delay(2000);
    driver_.epd_clear();
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    log_d("Displaying Another second text");
    driver_.epd_disp_string("Another second text", 350, 275);
    driver_.epd_update();
    log_d("Displaying Another third text without clear");
    drawRect(50, 50, 100, 100, BLACK);
    driver_.epd_disp_string("Another third text", 350, 375);
    driver_.epd_update();
    Delay(2000);
    driver_.epd_clear();
    driver_.epd_update();
    Delay(2000);*/
    return true;
}

void DisplayWaveshareUART43::updateDisplay(DisplaySystem::UpdateType updateType,
                                           bool clearFrameBuffer, int vcomNudge,
                                           DisplaySystem::UpdateType altUpdateType,
                                           ScreenSide screen, Rect altRect) {
    if (mirrorToSerial) {
        // MAGIC NUMBER FIX ME !
        USBSerial::getInstance().txSendData(
            reinterpret_cast<const uint8_t *>(gfxCanvas_.getBuffer()),
            WAVESHAREUART43_X_PIXELS * WAVESHAREUART43_Y_PIXELS / 8);
    }

    // Ignore all the non full updates now.

    if (updateType == DisplaySystem::UpdateType::FULL ||
        updateType == DisplaySystem::UpdateType::PARTIAL) {
        // base_draw();
        log_i("updating the display");
        driver_.epd_update();
    }

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        driver_.epd_clear();
        fillScreen(bg);
    }
}