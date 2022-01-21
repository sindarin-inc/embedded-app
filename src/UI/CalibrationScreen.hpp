#include "Bitmaps/Calibration.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Logo.hpp"

class CalibrationScreen : public BitmapScreen {
public:
    CalibrationScreen(Renderer &renderer) : BitmapScreen(renderer, LOGO_BITS) {}
    ~CalibrationScreen() override = default;
};
