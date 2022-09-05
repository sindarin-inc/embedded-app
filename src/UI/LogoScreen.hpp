#include "Bitmaps/Calibration.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Logo.hpp"

#define LOGO_O_RADIUS 30
#define LOGO_CHARACTER_SPACING 6
#define LOGO_L_HEIGHT 80
#define LOGO_L_WIDTH 34

class LogoScreen : public BitmapScreen {
public:
    LogoScreen() : BitmapScreen(LOGO2_BITS, LOGO2_WIDTH, LOGO2_HEIGHT) {}
    ~LogoScreen() override = default;

    void render(DisplaySystem &display) override {
        display.fillScreen(display.bg);
        // Regular S

        display.fillCircle(display.width() / 2 - LOGO_O_RADIUS * 2 - LOGO_CHARACTER_SPACING,
                           display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS, LOGO_O_RADIUS,
                           display.fg);
        display.fillRect(display.width() / 2 - LOGO_O_RADIUS * 2 - LOGO_CHARACTER_SPACING,
                         display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS * 2,
                         LOGO_O_RADIUS + 1, LOGO_O_RADIUS, display.fg);

        display.fillRect(display.width() / 2 - LOGO_O_RADIUS * 3 - LOGO_CHARACTER_SPACING,
                         display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS,
                         LOGO_O_RADIUS + 1, LOGO_O_RADIUS + 1, display.fg);

        // O
        display.fillCircle(display.width() / 2,
                           display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS, LOGO_O_RADIUS,
                           display.fg);

        // L

        display.fillRect(display.width() / 2 + LOGO_O_RADIUS + LOGO_CHARACTER_SPACING,
                         display.height() / 2 - LOGO_L_HEIGHT / 2, LOGO_L_WIDTH, LOGO_L_HEIGHT,
                         display.fg);

        display.updateDisplay();
    }
};

// Elongated S
/*
        display.fillCircle(display.width() / 2 - LOGO_O_RADIUS * 2 - LOGO_CHARACTER_SPACING,
                           display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS,
   LOGO_O_RADIUS, display.fg);

        display.fillCircle(display.width() / 2 - LOGO_O_RADIUS * 2 - LOGO_CHARACTER_SPACING,
                           display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS -
                               ((LOGO_L_HEIGHT - LOGO_O_RADIUS * 2) / 2),
                           LOGO_O_RADIUS, display.fg);
        display.fillRect(display.width() / 2 - LOGO_O_RADIUS * 2 - LOGO_CHARACTER_SPACING,
                         display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS * 2 -
                             ((LOGO_L_HEIGHT - LOGO_O_RADIUS * 2) / 2),
                         LOGO_O_RADIUS + 1,
                         LOGO_O_RADIUS + ((LOGO_L_HEIGHT - LOGO_O_RADIUS * 2) / 2),
   display.fg);

        display.fillRect(display.width() / 2 - LOGO_O_RADIUS * 3 - LOGO_CHARACTER_SPACING,
                         display.height() / 2 + LOGO_L_HEIGHT / 2 - LOGO_O_RADIUS -
                             ((LOGO_L_HEIGHT - LOGO_O_RADIUS * 2) / 2),
                         LOGO_O_RADIUS + 1,
                         LOGO_O_RADIUS + 1 + ((LOGO_L_HEIGHT - LOGO_O_RADIUS * 2) / 2),
   display.fg);
                         */
// display.fillRect();
//    display.drawXBitmap((display.width() - width_) / 2, (display.height() - height_) / 2,
//                        bitmap_, width_, height_, display.fg);