#include "PopupMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Bitmaps/Popup_Menu_Blank.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"

#define BATTERY_WIDTH 24
#define BATTERY_HEIGHT 9

// draw a battery with upper left at x, y and percentage full 'percentage' (out of 100)
void PopupMenuScreen::drawBattery(DisplaySystem &display, int x, int y, int percentage) {
    // We have 20 lines to represent so every 5% is 1 line.
    int numBatteryLines = percentage / 5;

    // Border
    display.drawRect(x, y, BATTERY_WIDTH, BATTERY_HEIGHT, display.fg);
    display.drawRect(x + 1, y + 1, BATTERY_WIDTH - 2, BATTERY_HEIGHT - 2, display.fg);

    // Tip of battery
    display.drawRect(x + BATTERY_WIDTH, y + 3, 2, 3, display.fg);

    // Draw percentage lines
    for (int i = 0; i < numBatteryLines; i++) {
        display.drawLine(x + 2 + i, y + 2, x + 2 + i, y + BATTERY_HEIGHT - 2, display.fg);
    }
}

void PopupMenuScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }
    if (!needsRerender) {
        return;
    }
    const int edgeOffset = 5;
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

    display.setFont(display.style.getFontFamily().popupFont);

    // Now draw!
    uint16_t textWidth = 0, textHeight = 0;
    auto batteryPercentage = this->power_.getBatteryPercentage();
    auto batteryStatus = this->power_.getBatteryPercentageOnlyString();
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    // These magic numbers are just based on trying to make it look right
    drawBattery(display, (screenWidth / 2) - 12, (screenHeight / 2) + 1, (int)batteryPercentage);

    display.getTextBounds(batteryStatus.c_str(), &textWidth, &textHeight);

    display.setFont(display.style.getFontFamily().popupFont);
    display.getTextBounds(batteryStatus.c_str(), &textWidth, &textHeight);

    // Similar to the above, magic numbers are just to make it look nice
    display.drawText(batteryStatus.c_str(), (screenWidth / 2) - (textWidth / 2) + 3,
                     (screenHeight / 2) + (textHeight / 2) - 9, screenWidth, screenHeight);

    size_t place = renderer_.getCurrentPlace();
    size_t lengthOfFile = renderer_.size();

    float placeInTextPercentage = (float)place / (float)lengthOfFile * 100;
    std::string placeString = StringFormat("%.0f%%", placeInTextPercentage);

    std::string bottomStr =
        std::string("Seek ") + std::string("(") + placeString + std::string(")");

    display.getTextBounds(bottomStr.c_str(), &textWidth, &textHeight);
    display.drawText(bottomStr, (screenWidth / 2) - (textWidth / 2),
                     (screenHeight / 2) + (textHeight / 2) + POPUP_MENU_BLANK_HEIGHT / 3,
                     screenWidth, screenHeight);

    auto topStr = "Text";
    display.getTextBounds(topStr, &textWidth, &textHeight);
    display.drawText(topStr, (screenWidth / 2) - (textWidth / 2),
                     (screenHeight / 2) - POPUP_MENU_BLANK_HEIGHT / 3, screenWidth, screenHeight);

    display.getTextBounds(leftButtonTitle_.c_str(), &textWidth, &textHeight);
    display.drawText(leftButtonTitle_.c_str(),
                     (display.width() - POPUP_MENU_BLANK_WIDTH) / 2 - textWidth / 2,
                     (screenHeight / 2) + 1, screenWidth, screenHeight);

    auto rightStr = "Invert";
    display.getTextBounds(rightStr, &textWidth, &textHeight);
    display.drawText(rightStr,
                     (display.width() - ((display.width() - POPUP_MENU_BLANK_WIDTH) / 2)) -
                         textWidth / 2,
                     (screenHeight / 2) + 1, screenWidth, screenHeight);

    display.updateDisplay();
    display.setAutoUpdate(autoUpdate);
    display.setFont(display.style.getFontFamily().readableFont);
    needsRerender = false;
}

void PopupMenuScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    auto back = [this]() {
        if (backCallback != nullptr) {
            needsRerender = true;
            backCallback();
        }
        needsRerender = true;
    };

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        if (leftButtonCallback_) {
            leftButtonCallback_();
        }
        break;
    case RemoteCommand::CHANNEL_PLUS:
        // Reserved for future use? Potentially lighting up and down?
        this->renderer_.getDisplay()->flipInversion();
        // When we invert, we should do a full clear of the display to make sure
        // the blacks are really gone.
        this->renderer_.getDisplay()->clearDisplay();
        prefs_.invertSetting.set(this->renderer_.getDisplay()->getInversion());
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        back();
        break;
    case RemoteCommand::NEXT:
        readableSeekScreen_.firstLoad = true;
        setCurrentScreen(readableSeekScreen_);
        break;
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::NOP_BUTTON:
        needsRerender = true;
        break;
    default:
        break;
    }
}
