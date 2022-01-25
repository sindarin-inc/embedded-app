#include "Bitmaps/Calibration.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Logo.hpp"

class LogoScreen : public BitmapScreen {
public:
    LogoScreen() : BitmapScreen(LOGO_BITS, LOGO_WIDTH, LOGO_HEIGHT) {}
    ~LogoScreen() override = default;
};
