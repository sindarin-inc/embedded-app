#include "Bitmaps/Calibration.hpp"
#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class CalibrationScreen : public Screen {
public:
    CalibrationScreen() : Screen() {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;
};
