#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#include "Displays/EinkET013TT1.hpp"
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256

class DisplayEinkET013TT1 : public DisplaySystem {
public:
    DisplayEinkET013TT1(uint8_t csLeft, uint8_t csRight, uint8_t oei, uint8_t rst,
                        uint8_t busy) noexcept
        : DisplaySystem(), gfxCanvas_(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS),
          driver_(csLeft, csRight, oei, rst, busy) {
        gfxClass_ = &gfxCanvas_;
    };

    ~DisplayEinkET013TT1() = default;

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED) override;

    void clearDisplay() override { fullRefreshNext_ = true; };

    // For those moments where you need to expose the driver directly
    auto getDriver() -> EinkET013TT1 * { return &driver_; }

protected:
    GFXcanvas1 gfxCanvas_;

    EinkET013TT1 driver_;
    bool fullRefreshNext_ = false;
};

#endif