#pragma once

#include <functional>
#include <utility>

#include "Ltr329Als.hpp"
#include "Misc/MovingAverage.hpp"

class LightingController {
public:
    LightingController() : lightSensor_(0), overrideValue_(-1) {}

    // Simple diagnostic test to check if the ambient light sensor is reporting the correct part id
    // and manufacturer id
    auto checkSensorAvailable() -> bool;

    // Enable updates and use the ambient light sensor to adjust the lighting
    void begin();

    // Disable lights altogether
    void disableLighting();

    // Enable lights altogether
    void enableLighting();

    // Disable updates and keeps a specific lighting setting (for when the user adjusts the setting
    // manually)
    void setOverrideValue(int percent);

    // Allow lighting only one of the screens, or both
    enum Screens {
        LEFT = 1 << 0,
        RIGHT = 1 << 1,
        BOTH = LEFT | RIGHT,
    };
    void setScreenMask(Screens screens, int fadeMs = 200);

    // This should happen periodically to check the ambient light and adjust the screen lighting
    // accordingly
    void checkAmbientLight(int fadeMs = 1000, int timeoutMs = 500);

    auto getCurrentSetting() -> int { return currentSetting_; }

protected:
    Ltr329Als lightSensor_;
    // Adjusting the number of samples here will change the responsiveness of the ambient light
    // adjustments
    MovingAverage<int, 8> movingAverage_;

    int currentSetting_ = 0;
    int overrideValue_;
    bool lightsOut_;

    void setLighting(int percent, int fadeMs);

    Screens screens_ = BOTH;
};
