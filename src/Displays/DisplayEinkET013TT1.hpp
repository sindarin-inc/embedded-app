#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#if SIMULATOR
#include "Displays/FakeEinkET013TT1.hpp"
#else
#include "Displays/EinkET013TT1.hpp"
#endif
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256

class DisplayEinkET013TT1 : public DisplaySystem {
public:
    DisplayEinkET013TT1(uint8_t csLeft, uint8_t csRight, uint8_t rst) noexcept
        : gfxCanvas_(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS), driver_(csLeft, csRight, rst) {
        gfxClass_ = &gfxCanvas_;
        canvasBuffer_ = gfxCanvas_.getBuffer();
        canvasWidth_ = EinkET013TT1_X_PIXELS;
        canvasHeight_ = EinkET013TT1_Y_PIXELS;
    };
    DisplayEinkET013TT1(DisplayEinkET013TT1 const &) = delete;

    ~DisplayEinkET013TT1() override = default;

    auto setup() -> bool override;

    // TODO: Probably this is a good candidate for a struct for params since there are a lot of
    // values that can be passed and some may be unset defaults.
    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

    void beginPowerOn() override { driver_.beginPowerOn(); };

    void clearDisplay() override { fullRefreshNext_ = true; };

    void clearFrameBuffer() override { fillScreen(bg); };
    void invertDisplay(bool newInvert) override;

    // For those moments where you need to expose the driver directly
    auto getDriver() -> EinkET013TT1 * { return &driver_; }

    void drawFullscreenBitmapFromFile(RendererStream &stream) override;

    [[nodiscard]] auto darkPictureVComAdj(uint8_t darkPixelsPercent) const -> int override {
        return darkPixelsPercent > 40 ? -2500 : 0;
    }

protected:
    GFXcanvas1 gfxCanvas_;

    EinkET013TT1 driver_;
    bool fullRefreshNext_ = false;
    uint16_t au4Count_ = 0;
    uint16_t numPartials_ = 0;
    unsigned long firstPartialTime_ = 0;
};

#endif