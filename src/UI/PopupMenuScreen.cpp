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
    display.drawXBitmap(
        (int16_t)(display.localXOffset + (display.width() - POPUP_MENU_BLANK_WIDTH) / 2),
        (int16_t)(display.localYOffset + (display.height() - POPUP_MENU_BLANK_HEIGHT) / 2),
        popupMenuBlankBits, POPUP_MENU_BLANK_WIDTH, POPUP_MENU_BLANK_HEIGHT, display.fg);

    display.setFont(&LUCIDA_GRANDE6PT7B);

    // Now draw!
    uint16_t width = 0, height = 0;
    auto batteryStatus = this->power_.getBatteryPercentageOnlyString();

    display.getTextBounds(batteryStatus.c_str(), &width, &height);
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    display.setFont(&LUCIDA_GRANDE5PT7B);
    display.getTextBounds(batteryStatus.c_str(), &width, &height);

    display.drawText(batteryStatus.c_str(), display.localXOffset + (screenWidth / 2) - (width / 2),
                     display.localYOffset + (screenHeight / 2) + (height / 2) + 10, screenWidth,
                     screenHeight);

    size_t place = fileRenderer_->getCurrentPlace();
    size_t lengthOfFile = fileRenderer_->size();

    float placeInTextPercentage = (float)place / (float)lengthOfFile * 100;
    std::string placeString = StringFormat("%.0f%%", placeInTextPercentage);

    display.getTextBounds(placeString.c_str(), &width, &height);
    display.drawText(placeString.c_str(), display.localXOffset + (screenWidth / 2) - (width / 2),
                     display.localYOffset + (screenHeight / 2) + (height / 2) - 10, screenWidth,
                     screenHeight);

    display.getTextBounds("Settings", &width, &height);
    display.drawText("Settings", display.localXOffset + (screenWidth / 2) - (width / 2),
                     display.localYOffset + (screenHeight / 2) + (height / 2) +
                         POPUP_MENU_BLANK_HEIGHT / 3,
                     screenWidth, screenHeight);

    display.getTextBounds("Text", &width, &height);
    display.drawText("Text", display.localXOffset + (screenWidth / 2) - (width / 2),
                     display.localYOffset + (screenHeight / 2) - POPUP_MENU_BLANK_HEIGHT / 3,
                     screenWidth, screenHeight);

    display.getTextBounds("Menu", &width, &height);
    display.drawText(
        "Menu", display.localXOffset + (display.width() - POPUP_MENU_BLANK_WIDTH) / 2 - width / 2,
        display.localYOffset + (screenHeight / 2) + 1, screenWidth, screenHeight);

    display.getTextBounds("Invert", &width, &height);
    display.drawText("Invert",
                     display.localXOffset +
                         (display.width() - ((display.width() - POPUP_MENU_BLANK_WIDTH) / 2)) -
                         width / 2,
                     display.localYOffset + (screenHeight / 2) + 1, screenWidth, screenHeight);

    display.updateDisplay();
    display.setAutoUpdate(autoUpdate);
    needsRerender = false;
}