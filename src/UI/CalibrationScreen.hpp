#ifndef CALIBRATION_SCREEN_HPP
#define CALIBRATION_SCREEN_HPP

#include "Bitmaps/Calibration.hpp"
#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class CalibrationScreen : public Screen {
public:
    CalibrationScreen() : Screen() {}

    void drawSiemensStar(DisplaySystem &display, uint32_t originX, uint32_t originY,
                         uint32_t radius);

    void setOpticsTest() { opticsTest = true; }
    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool opticsTest = false;
};

#endif