#include "Bitmaps/Calibration.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Logo.hpp"

#define LOGO_O_RADIUS 80
#define LOGO_CHARACTER_SPACING 6
#define LOGO_L_HEIGHT 80
#define LOGO_L_WIDTH 34

//
// This is mostly deprecated as we don't show a splash screen
// anymore, but we may revisit this someday.
//

class LogoScreen : public BitmapScreen {
public:
    LogoScreen() : BitmapScreen(LOGO2_BITS, LOGO2_WIDTH, LOGO2_HEIGHT) {}
    ~LogoScreen() override = default;

    void render(DisplaySystem &display) override {
        display.fillScreen(display.bg);

        display.fillCircle(display.width() / 2.0, display.height() / 2, LOGO_O_RADIUS, display.fg);

        display.fillRect(display.width() / 2.0 - LOGO_O_RADIUS * 1.6 / 2 + 1,
                         display.height() / 2.0 - LOGO_O_RADIUS * 1.6 / 2 + 1, LOGO_O_RADIUS * 1.6,
                         LOGO_O_RADIUS * 1.6, display.bg);

        display.updateDisplay();
    }
};
