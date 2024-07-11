#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#if SIMULATOR
#include "Displays/FakeEinkET013TT1.hpp"
#else
#include "Displays/EinkET013TT1.hpp"
#endif
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256
#define EinkET013TT1_DEFAULT_X_MARGIN 30
#define EinkET013TT1_DEFAULT_Y_MARGIN 20
#define EinkET013TT1_MIN_X_MARGIN 5
#define EinkET013TT1_MIN_Y_MARGIN 5
#define EinkET013TT1_MIN_VIEWABLE_BOX_WIDTH 125
#define EinkET013TT1_MIN_VIEWABLE_BOX_HEIGHT 45

class DisplayEinkET013TT1 : public DisplaySystem {
public:
    DisplayEinkET013TT1(uint8_t csLeft, uint8_t csRight, uint8_t rst,
                        PreferencesStore &prefs) noexcept
        : gfxCanvas_(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS), driver_(csLeft, csRight, rst),
          prefs_(prefs) {
        gfxClass_ = &gfxCanvas_;
        canvasBuffer_ = gfxCanvas_.getBuffer();
        canvasWidth_ = EinkET013TT1_X_PIXELS;
        canvasHeight_ = EinkET013TT1_Y_PIXELS;
    };
    DisplayEinkET013TT1(DisplayEinkET013TT1 const &) = delete;

    ~DisplayEinkET013TT1() override = default;

    auto setup() -> bool override;

    virtual auto getDisplayWidth() -> uint16_t override { return EinkET013TT1_X_PIXELS; }
    virtual auto getDisplayHeight() -> uint16_t override { return EinkET013TT1_Y_PIXELS; }
    virtual auto getMinInsets() -> DisplayInset override {
        return {EinkET013TT1_MIN_X_MARGIN, EinkET013TT1_MIN_X_MARGIN, EinkET013TT1_MIN_Y_MARGIN,
                EinkET013TT1_MIN_Y_MARGIN};
    }
    virtual auto getDefaultInsets() -> DisplayInset override {
        return {EinkET013TT1_DEFAULT_X_MARGIN, EinkET013TT1_DEFAULT_X_MARGIN,
                EinkET013TT1_DEFAULT_Y_MARGIN, EinkET013TT1_DEFAULT_Y_MARGIN};
    }
    virtual auto setTemporaryInsets(DisplayInset newInsets) -> void override;
    virtual auto storeInsets() -> void override;

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

#if SIMULATOR
    auto renderQueue() -> QueueHandle_t override { return driver_.renderQueue(); }

    auto buffer() -> std::unique_ptr<uint8_t[]> & override { return driver_.buffer(); }

    // For the simulator to know if it should skip the waveform processing
    auto getSkipWaveform() -> bool override { return driver_.getSkipWaveform(); }

    void setSkipWaveform(bool skip) override { driver_.setSkipWaveform(skip); }

    // For the simulator to know if it should skip the signalling queue
    auto getSkipSignalling() -> bool override { return driver_.getSkipSignalling(); }

    void setSkipSignalling(bool skip) override { driver_.setSkipSignalling(skip); }
#endif

protected:
    void ensureBoundaries();
    GFXcanvas1 gfxCanvas_;
    EinkET013TT1 driver_;
    PreferencesStore &prefs_;
    bool fullRefreshNext_ = false;
    uint16_t au4Count_ = 0;
    uint16_t numPartials_ = 0;
    unsigned long firstPartialTime_ = 0;
};

#endif