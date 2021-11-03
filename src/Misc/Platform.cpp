#include "Platform.hpp"

#include "sindarin-debug.h"

const int MAX_ANALOG_VAL = 4095.0;
const float MAX_BATTERY_VOLTAGE = 4.2; // Max LiPoly voltage of a 3.7 battery is 4.2

#ifdef TINYPICO_SINDARIN
#include <TinyPICO.h>
TinyPICO tp;
#endif

auto get_battery_life() -> String {
#ifdef TINYPICO_SINDARIN
    float voltageLevel = tp.GetBatteryVoltage();
#else
    // A13 pin is not exposed on Huzzah32 board because it's tied to
    // measuring voltage level of battery. Note: you must
    // multiply the analogRead value by 2x to get the true battery
    // level. See:
    // https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/esp32-faq
    int rawValue = analogRead(A13);

    // Reference voltage on ESP32 is 1.1V
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration
    // See also: https://bit.ly/2zFzfMT
    float voltageLevel = (rawValue / MAX_ANALOG_VAL) * 2 * 1.1 * 3.3; // calculate voltage level
#endif
    float batteryFraction = voltageLevel / MAX_BATTERY_VOLTAGE;
    String battery_output = "  Percent: " + String(batteryFraction * 100) + "%";
    DEBUG_LN(battery_output.c_str());
    SHOW_HEAP();
    return battery_output;
}
