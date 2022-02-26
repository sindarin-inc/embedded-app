#include "PopupMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Bitmaps/Popup_Menu_Blank.hpp"
#include "Fonts/LucidaGrande10pt7b.h"
#include "Fonts/LucidaGrande14pt7b.h"
#include "Fonts/LucidaGrande5pt7b.h"
#include "Fonts/LucidaGrande6pt7b.h"
#include "Misc/StringUtil.hpp"

void PopupMenuScreen::render(DisplaySystem &display) {
    const int edgeOffset = 5;
    if (!needsRerender) {
        return;
    }
    log_d("PopupMenuScreen render");
    auto autoUpdate = display.getAutoUpdate();
    display.setAutoUpdate(false);
    // Blank out screen
    display.fillScreen(display.bg);
    // Draw menu surroundings
    display.drawXBitmap((int16_t)((display.width() - POPUP_MENU_BLANK_WIDTH) / 2),
                        (int16_t)((display.height() - POPUP_MENU_BLANK_HEIGHT) / 2),
                        popupMenuBlankBits, POPUP_MENU_BLANK_WIDTH, POPUP_MENU_BLANK_HEIGHT,
                        display.fg);

    display.setFont(&LUCIDA_GRANDE6PT7B);

    // Now draw!
    uint16_t textWidth = 0, textHeight = 0;
    auto batteryStatus = this->power_.getBatteryPercentageOnlyString();

    display.getTextBounds(batteryStatus.c_str(), &textWidth, &textHeight);
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    display.setFont(&LUCIDA_GRANDE5PT7B);
    display.getTextBounds(batteryStatus.c_str(), &textWidth, &textHeight);

    display.drawText(batteryStatus.c_str(), (screenWidth / 2) - (textWidth / 2),
                     (screenHeight / 2) + (textHeight / 2) + 10, screenWidth, screenHeight);

    size_t place = renderer_.getCurrentPlace();
    size_t lengthOfFile = renderer_.size();

    float placeInTextPercentage = (float)place / (float)lengthOfFile * 100;
    std::string placeString = StringFormat("%.0f%%", placeInTextPercentage);

    display.getTextBounds(placeString.c_str(), &textWidth, &textHeight);
    display.drawText(placeString.c_str(), (screenWidth / 2) - (textWidth / 2),
                     (screenHeight / 2) + (textHeight / 2) - 10, screenWidth, screenHeight);

    display.getTextBounds("Settings", &textWidth, &textHeight);
    display.drawText("Settings", (screenWidth / 2) - (textWidth / 2),
                     (screenHeight / 2) + (textHeight / 2) + POPUP_MENU_BLANK_HEIGHT / 3,
                     screenWidth, screenHeight);

    display.getTextBounds("Text", &textWidth, &textHeight);
    display.drawText("Text", (screenWidth / 2) - (textWidth / 2),
                     (screenHeight / 2) - POPUP_MENU_BLANK_HEIGHT / 3, screenWidth, screenHeight);

    display.getTextBounds(leftButtonTitle_.c_str(), &textWidth, &textHeight);
    display.drawText(leftButtonTitle_.c_str(),
                     (display.width() - POPUP_MENU_BLANK_WIDTH) / 2 - textWidth / 2,
                     (screenHeight / 2) + 1, screenWidth, screenHeight);

    display.getTextBounds("Invert", &textWidth, &textHeight);
    display.drawText("Invert",
                     (display.width() - ((display.width() - POPUP_MENU_BLANK_WIDTH) / 2)) -
                         textWidth / 2,
                     (screenHeight / 2) + 1, screenWidth, screenHeight);

    display.updateDisplay();
    display.setAutoUpdate(autoUpdate);
    needsRerender = false;
}