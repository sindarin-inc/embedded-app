#include "Bitmaps/Calibration.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Logo.hpp"

class LogoScreen : public BitmapScreen {
public:
    LogoScreen() : BitmapScreen(LOGO_BITS) {}
    ~LogoScreen() override = default;
};
