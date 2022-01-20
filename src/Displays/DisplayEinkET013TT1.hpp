#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include <Adafruit_GFX.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256

#define EinkET013TT1_WIDTH_IN_MM 23.3
#define EinkET013TT1_HEIGHT_IN_MM 23.3

#define EPD_TCON_FRAME_RATE_HZ 24

class DisplayEinkET013TT1 : public DisplaySystem, public Adafruit_GFX {
public:
    DisplayEinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy);
    ~DisplayEinkET013TT1() override = default;

    void drawBox(int xStart, int yStart, int xEnd, int yEnd) override;

    void drawText() override;
    void drawText(String newText) override;
    void drawText(String newText, int x, int y) override;

    void drawTextInBox(String newText, int startX, int startY, int endX, int endY) override;

    void drawCalibrationScreen(int startX, int startY, int endX, int endY);

    void getTextBounds(const char *buffer, uint16_t *w, uint16_t *h);
    auto getTextHeight() -> int override;
    auto getTextHeight(const char *buffer) -> int;
    auto getTextWidth(const char *buffer) -> int override;

    void setPartialRefresh() override;
    void unsetPartialRefresh() override;
    auto getPartialRefresh() -> int override;

    void setAutoUpdate(bool update);
    auto getAutoUpdate() -> bool;

    void displaySplashScreen() override;

    void drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) override {
        assert(0);
    }

    auto getFont() -> const GFXfont *;
    void setFont(const GFXfont *newFont);

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    virtual void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                             uint16_t color);
    void updateDisplay();
    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setFullWindow() {
        unsetPartialRefresh();
        partialX_ = 0;
        partialY_ = 0;
        partialW_ = WIDTH;
        partialH_ = HEIGHT;
    }
    void fillScreen(uint16_t color) override // 0x0 black, >0x0 white, to buffer
    {
        // Clamp to black or white
        uint8_t data = (color == 0) ? color : 0xFF;
        for (unsigned char &x : buffer_) {
            x = data;
        }
    }

    uint16_t bg = 0x0000; // 0xFFFF;
    uint16_t fg = 0xFFFF; // 0x0000;

protected:
    bool autoUpdate_;

    void drawTextCmdsOnly(const String &newText, int x, int y, int lineHeight);
    void drawBoxCmdsOnly(int xStart, int yStart, int xEnd, int yEnd);

    static inline auto min(uint16_t a, uint16_t b) -> uint16_t { return (a < b ? a : b); };
    static inline auto max(uint16_t a, uint16_t b) -> uint16_t { return (a > b ? a : b); };
    template <typename T> static inline void swap(T &a, T &b) {
        T temp = a;
        a = b;
        b = temp;
    };

    uint16_t partialX_, partialY_, partialW_, partialH_;
    uint8_t buffer_[(EinkET013TT1_X_PIXELS / 8) * EinkET013TT1_Y_PIXELS];
    void rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h) {
        switch (getRotation()) {
        case 1:
            swap(x, y);
            swap(w, h);
            x = WIDTH - x - w;
            break;
        case 2:
            x = WIDTH - x - w;
            y = HEIGHT - y - h;
            break;
        case 3:
            swap(x, y);
            swap(w, h);
            y = HEIGHT - y - h;
            break;
        }
    }
    void writeImage(uint8_t *fb, const unsigned char *img, uint16_t size);
    auto writePartialImage(uint16_t x, uint16_t y, const unsigned char *img, uint16_t imgW,
                           uint16_t imgH) -> uint8_t;
};

#endif