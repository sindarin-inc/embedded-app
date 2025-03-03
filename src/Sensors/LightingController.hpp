#pragma once

#include <functional>
#include <utility>

#if CONFIG_USE_LTR329
#include "Ltr329Als.hpp"
#endif
#include "Misc/MovingAverage.hpp"
#include "Storage/PreferencesStore.hpp"

class LightingController {
public:
    LightingController(PreferencesStore &prefs)
        : prefs_(prefs),
#if CONFIG_USE_LTR329
          lightSensor_(0),
#endif
          movingAverage_(25) {
    }
    LightingController(LightingController const &) = delete;

    void pause() { paused_ = true; }
    void resume() { paused_ = false; }

    // Simple diagnostic test to check if the ambient light sensor is reporting the correct part id
    // and manufacturer id
    auto checkSensorAvailable() -> bool;

    // Enable updates and use the ambient light sensor to adjust the lighting
    void begin();

    // Disable lights altogether
    void shutdown(int fadeMs = 200);

    // Disable updates and keeps a specific lighting setting (for when the user adjusts the setting
    // manually)
    void setOverrideValuePercent(int percent, bool userInitiated = false);

    [[nodiscard]] auto getCurrentDutyCycle() const -> int;
    [[nodiscard]] auto getCurrentPercent() const -> int;

    // Allow lighting only one of the screens, or both
    enum Screens {
        LEFT = 1 << 0,
        RIGHT = 1 << 1,
        BOTH = LEFT | RIGHT,
    };
    void setScreenMask(Screens screens, int fadeMs = 200);

    auto readAmbientLightData(uint16_t &channel1, uint16_t &channel0) -> bool;

    auto readPredictedDutyCycle() -> int;

    // This should happen periodically to check the ambient light and adjust the screen lighting
    // accordingly
    void checkAmbientLight(int fadeMs = 1000, int timeoutMs = 100);

protected:
    auto luxify(uint16_t channel0, uint16_t channel1) -> float;
    auto scaleLux(float lux) -> float;
    PreferencesStore &prefs_;
#if CONFIG_USE_LTR329
    Ltr329Als lightSensor_;
#endif
    // Adjusting the number of samples here will change the responsiveness of the ambient light
    // adjustments
    MovingAverage<int, 5> movingAverage_;
    bool enabled_ = false;

    bool paused_ = false;

    // Duty cycle as predicted by the ambient light sensor
    int predictedDutyCycle_ = 0;
    // Duty cycle as overridden by the user
    int overrideDutyCycle_ = -1;

    void setCurrentDutyCycle(int dutyCycle, int fadeMs);

    Screens screens_ = BOTH;
};
