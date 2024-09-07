#ifndef DISPLAY_EINK_047TC1_H
#define DISPLAY_EINK_047TC1_H

#include "Displays/DisplaySystem.hpp"
#include "sindarin-debug.h"

extern "C" {
#include "ed047tc1.h"
#include "epd_driver.h"
}

class DisplayEink047TC1 : public DisplaySystem {
public:
    DisplayEink047TC1() noexcept : gfxCanvas_(EPD_WIDTH, EPD_HEIGHT) { gfxClass_ = &gfxCanvas_; };
    DisplayEink047TC1(DisplayEink047TC1 const &) = delete;

    ~DisplayEink047TC1() override = default;

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {30, 30, 20, 20};
    };
    [[nodiscard]] auto minTextInsets() const -> DisplayInset override { return {5, 5, 5, 5}; };
    [[nodiscard]] auto minTextBoxWidth() const -> int16_t override { return 125; };
    [[nodiscard]] auto minTextBoxHeight() const -> int16_t override { return 45; };

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

    void beginPowerOn() override;

    void clearDisplay() override;

    void clearFrameBuffer() override { fillScreen(bg); };
    void invertDisplay(bool newInvert) override;

    void drawFullscreenBitmapFromFile(RendererStream &stream) override;

    [[nodiscard]] auto darkPictureVComAdj(uint8_t darkPixelsPercent) const -> int override {
        return 0; // Adjust if needed
    }

    void sendCurrentScreenToSerial(bool screenshot = false) const override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return EPD_WIDTH; }
    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return EPD_HEIGHT; }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    void sendBufferToSerial(const uint8_t *buffer, bool screenshot = false) const;

    GFXcanvas1 gfxCanvas_;
    // GFXcanvas4 gfxCanvas_;
};

#endif
