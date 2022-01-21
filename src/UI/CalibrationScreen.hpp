#include "Bitmaps/Calibration.hpp"
#include "Displays/DisplaySystem.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Logo.hpp"

class CalibrationScreen : public BitmapScreen {
public:
    CalibrationScreen(DisplaySystem &display) : BitmapScreen(display, LOGO_BITS) {}
    ~CalibrationScreen() override = default;
};
