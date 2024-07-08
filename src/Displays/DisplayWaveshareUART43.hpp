#pragma once

#include "DisplaySystem.hpp"
#if SIMULATOR
#include "Displays/FakeEinkET013TT1.hpp"
#else
#include "Displays/WaveshareUART43.hpp"
#endif
#include "sindarin-debug.h"

#define WAVESHAREUART43_X_PIXELS 800
#define WAVESHAREUART43_Y_PIXELS 600

class DisplayWaveshareUART43 : public DisplaySystem {
public:
    DisplayWaveshareUART43() : gfxCanvas_(WAVESHAREUART43_X_PIXELS, WAVESHAREUART43_Y_PIXELS) {
        gfxClass_ = &gfxCanvas_;
        canvasBuffer_ = gfxCanvas_.getBuffer();
        canvasWidth_ = WAVESHAREUART43_X_PIXELS;
        canvasHeight_ = WAVESHAREUART43_Y_PIXELS;
    };
    DisplayWaveshareUART43(DisplayWaveshareUART43 const &) = delete;
    auto setup() -> bool override;
    ~DisplayWaveshareUART43() override = default;
    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

    void fillScreen(uint16_t color) override;
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint16_t color) override;

    void setFont(Font *f) override;

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override;

    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override;

    void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius,
                       uint16_t color) override;

    void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius,
                       uint16_t color) override;

    void setRotation(uint8_t r) override;

    void setCursor(int16_t x, int16_t y) override;

    void drawBox(int startX, int startY, int endX, int endY) override;

    // Draw a dotted box
    void drawDottedBox(int startX, int startY, int endX, int endY, int dotSize = 1,
                       int gapSize = 1) override;

    // Draw a bitmap
    void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                     uint16_t color) override;

    // Draw a bitmap scaled by a multiplier
    void drawScaledXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                           uint16_t color, int scale) override;

    void drawPBM(int16_t x, int16_t y, const std::string &fname, FileSystem &fs) override;
    void drawPBM(int16_t x, int16_t y, const uint8_t *pbmBytes, int size) override;
    void drawPBM(int16_t x, int16_t y, RendererStream &stream) override;

    // override for subclass, need access to raw drawing buffer
    virtual void drawFullscreenBitmapFromFile(RendererStream &stream) override {
        log_i("not supported");
    }

    void drawSingleLineOfText(const std::string &line, int x, int y,
                              bool inverted = false) override;

    // Clear the whole screen
    //  void clearScreen();

protected:
    GFXcanvas1 gfxCanvas_;
    void draw_text_demo();
    WaveshareUART43 driver_;
};