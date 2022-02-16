#ifndef CALIBRATION_SCREEN_HPP
#define CALIBRATION_SCREEN_HPP

#include "Bitmaps/Calibration.hpp"
#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class CalibrationScreen : public Screen {
public:
    CalibrationScreen() : Screen() {}

    void drawSiemensStar(DisplaySystem &display, u_int32_t originX, u_int32_t originY,
                         u_int32_t radius);

    void setOpticsTest() { opticsTest = true; }
    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool opticsTest = false;
};

#endif