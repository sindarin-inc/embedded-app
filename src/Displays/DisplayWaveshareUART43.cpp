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

template <typename T>
auto DisplayWaveshareUART43::println(T in) -> size_t {
    return DisplaySystem::println(in);
}

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

void DisplayWaveshareUART43::clearScreen() { DisplaySystem::clearScreen(); }

void DisplayWaveshareUART43::drawSingleLineOfText(const std::string &line, int x, int y,
                                                  bool inverted) {
    DisplaySystem::drawSingleLineOfText(line, x, y, inverted);
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
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

    driver_.epd_udpate();
    Delay(2000);
}

auto DisplayWaveshareUART43::setup() -> bool {

    DisplaySystem::setup();
    driver_.epd_init();
    driver_.epd_wakeup();
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_clear();
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    driver_.epd_disp_string("Sol Reader", 350, 275);
    driver_.epd_udpate();
    Delay(1000);
    driver_.epd_reset();
    driver_.epd_wakeup();
    driver_.epd_clear();
    return true;
}

void DisplayWaveshareUART43::updateDisplay(DisplaySystem::UpdateType updateType,
                                           bool clearFrameBuffer, int vcomNudge,
                                           DisplaySystem::UpdateType altUpdateType,
                                           ScreenSide screen, Rect altRect) {
    if (mirrorToSerial) {
        // MAGIC NUMBER FIX ME !
        USBSerial::getInstance().txSendData(
            reinterpret_cast<const uint8_t *>(gfxCanvas_.getBuffer()), 8192);
    }

    // Ignore all the non full updates now.

    if (updateType == DisplaySystem::UpdateType::FULL) {
        // base_draw();
        log_i("updating the display") driver_.epd_udpate();
    }

    driver_.epd_reset();
    driver_.epd_wakeup();
    driver_.epd_clear();
}