#pragma once

#include <functional>
#include <utility>

#include "Ltr329Als.hpp"
#include "Misc/MovingAverage.hpp"

class LightingController {
public:
    LightingController() : lightSensor_(0) {}
    LightingController(LightingController const &) = delete;

    // Simple diagnostic test to check if the ambient light sensor is reporting the correct part id
    // and manufacturer id
    auto checkSensorAvailable() -> bool;

    // Enable updates and use the ambient light sensor to adjust the lighting
    void begin();

    // If lighting is overridden, turn ambient light sensor off, since it was turned on for
    // telemetry
    void returnToDisabledState();

    // Disable lights altogether
    void disableLighting(int fadeMs = 200);

    // Enable lights altogether
    void enableLighting(int fadeMs = 200);

    // Disable updates and keeps a specific lighting setting (for when the user adjusts the setting
    // manually)
    void setOverrideValuePercent(int percent, bool maintainLightSensor = false);
    auto getCurrentPercent() -> int;

    // Allow lighting only one of the screens, or both
    enum Screens {
        LEFT = 1 << 0,
        RIGHT = 1 << 1,
        BOTH = LEFT | RIGHT,
    };
    void setScreenMask(Screens screens, int fadeMs = 200);

    auto readAmbientLightData(uint16_t &channel1, uint16_t &channel0) -> bool;

    // This should happen periodically to check the ambient light and adjust the screen lighting
    // accordingly
    void checkAmbientLight(int fadeMs = 1000, int timeoutMs = 100);

protected:
    Ltr329Als lightSensor_;
    // Adjusting the number of samples here will change the responsiveness of the ambient light
    // adjustments
    MovingAverage<int, 5> movingAverage_;

    int currentDutyCycle_ = 0;
    int overrideDutyCycle_{-1};

    // enabled_ refers to the ambient light sensor being turned on, not the lights themselves
    bool enabled_ = false;
    // lightsOut refers to the lights being turned off
    bool lightsOut_;

    void setCurrentDutyCycle(int dutyCycle, int fadeMs);
    auto getCurrentDutyCycle() -> int { return currentDutyCycle_; }

    Screens screens_ = BOTH;
};
