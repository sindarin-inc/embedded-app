#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#include "Displays/EinkET013TT1.hpp"
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256

class DisplayEinkET013TT1 : public DisplaySystem {
public:
    DisplayEinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy);
    ~DisplayEinkET013TT1() override = default;

    void updateDisplay() override;

    void clearDisplay() override;

    // For those moments where you need to expose the driver directly
    auto getDriver() -> EinkET013TT1 & { return driver_; }

protected:
    EinkET013TT1 driver_;
};

#endif