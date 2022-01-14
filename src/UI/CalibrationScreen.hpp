#include "Bitmaps/Calibration.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/BitmapScreen.hpp"

class CalibrationScreen : public BitmapScreen {
public:
    CalibrationScreen(Renderer &renderer) : BitmapScreen(renderer, Logo_bits) {}
    ~CalibrationScreen() = default;
};
