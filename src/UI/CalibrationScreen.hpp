#include "UI/BitmapScreen.hpp"

#include "Renderers/Renderer.hpp"
#include "Bitmaps/Calibration.hpp"

class CalibrationScreen : public BitmapScreen {
public:
    CalibrationScreen(Renderer &renderer) : BitmapScreen(renderer, Calibration_bits) {}
    ~CalibrationScreen() = default;
};
