#include "PopupMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Bitmaps/Popup_Menu_Blank.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#define BATTERY_WIDTH 24
#define BATTERY_HEIGHT 9

#define POPUP_MENU_RADIUS 40

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

    log_d("PopupMenuScreen render");

    // Blank out screen
    display.fillScreen(display.bg);

    display.setFont(display.style.getFontFamily().popupFont);

    // Now draw!
    uint16_t textWidth = 0, textHeight = 0;
    auto batteryPercentage = this->power_.getBatteryPercentage();
    auto batteryStatus = this->power_.getBatteryPercentageOnlyString();
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    uint16_t heightOffset = display.getTextHeight();

    // These magic numbers are just based on trying to make it look right
    drawBattery(display, (screenWidth / 2) - 12, (screenHeight / 2) - 3, (int)batteryPercentage);

    display.getTextBounds(batteryStatus.c_str(), &textWidth, &textHeight);

    display.setFont(display.style.getFontFamily().popupFont);
    display.getTextBounds(batteryStatus.c_str(), &textWidth, &textHeight);

    // Similar to the above, magic numbers are just to make it look nice
    display.drawSingleLineOfText(batteryStatus.c_str(), (screenWidth / 2) - (textWidth / 2),
                                 (screenHeight / 2) + (textHeight / 2) - 9 - heightOffset);

    size_t place = renderer_.getCurrentPlace();
    size_t lengthOfFile = renderer_.size();

    float placeInTextPercentage = (float)place / (float)lengthOfFile * 100;
    std::string placeString = StringFormat("%.1f%%", placeInTextPercentage);

    // We want to square the items if we can.
    int yDirOffset = display.height() / 2 - display.width() / 2;
    if (yDirOffset < 0) {
        yDirOffset = 0;
    }

    std::string bottomStr =
        std::string("Seek ") + std::string("(") + placeString + std::string(")");

    display.getTextBounds(bottomStr.c_str(), &textWidth, &textHeight);
    display.drawSingleLineOfText(bottomStr, (screenWidth / 2) - (textWidth / 2),
                                 display.height() - yDirOffset - arrowWidth_ - arrowTextSpacing_ -
                                     textHeight);

    // Left Arrow
    display.drawLine(0, screenHeight / 2, arrowWidth_, screenHeight / 2 + arrowHeight_ / 2,
                     display.fg);
    display.drawLine(0, screenHeight / 2, arrowWidth_, screenHeight / 2 - arrowHeight_ / 2,
                     display.fg);

    // Down Arrow
    display.drawLine(display.width() / 2, display.height() - yDirOffset,
                     display.width() / 2 - arrowHeight_ / 2,
                     display.height() - yDirOffset - arrowWidth_, display.fg);
    display.drawLine(display.width() / 2, display.height() - yDirOffset,
                     display.width() / 2 + arrowHeight_ / 2,
                     display.height() - yDirOffset - arrowWidth_, display.fg);

    auto topStr = "Text";
    display.getTextBounds(topStr, &textWidth, &textHeight);
    display.drawSingleLineOfText(topStr, (screenWidth / 2) - (textWidth / 2),
                                 yDirOffset + arrowWidth_ + arrowTextSpacing_);

    // Top Arrow
    display.drawLine(display.width() / 2, yDirOffset, display.width() / 2 - arrowHeight_ / 2,
                     yDirOffset + arrowWidth_, display.fg);
    display.drawLine(display.width() / 2, yDirOffset, display.width() / 2 + arrowHeight_ / 2,
                     yDirOffset + arrowWidth_, display.fg);

    display.getTextBounds(leftButtonTitle_.c_str(), &textWidth, &textHeight);
    display.drawSingleLineOfText(leftButtonTitle_.c_str(), arrowWidth_ + arrowTextSpacing_,
                                 (screenHeight / 2) - textHeight / 2 - 1);

    // Right Arrow
    display.drawLine(display.width(), screenHeight / 2, display.width() - arrowWidth_,
                     screenHeight / 2 + arrowHeight_ / 2, display.fg);
    display.drawLine(display.width(), screenHeight / 2, display.width() - arrowWidth_,
                     screenHeight / 2 - arrowHeight_ / 2, display.fg);

    auto rightStr = "Invert";
    display.getTextBounds(rightStr, &textWidth, &textHeight);
    display.drawSingleLineOfText(rightStr,
                                 (display.width() - arrowWidth_ - arrowTextSpacing_ - textWidth),
                                 (screenHeight / 2) - textHeight / 2 - 3);

    display.updateDisplay();
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
            this->renderer_.getDisplay()->clearDisplay();
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
    case RemoteCommand::ANIMATION:
        needsRerender = false;
        break;
    default:
        break;
    }
}
