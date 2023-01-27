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

auto PopupMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    // Later :)
    // context.setViewFrame(*clockView_, {{0, 0}, size},
    //                     LayoutOptionsSizeToFit | LayoutOptionsAlignLeft |
    //                         LayoutOptionsAlignBottom);

    context.setViewFrame(placeInReadableView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                             LayoutOptionsAlignBottom);

    context.setSublayoutFrame(
        {{0, 0}, size}, LayoutOptionsSizeToFit | LayoutOptionsAlignBottom | LayoutOptionsAlignRight,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            auto iconRect = context.setViewFrame(
                batteryIconView_, {{x, y}, {size.width, size.height}}, LayoutOptionsSizeToFit);
            x += iconRect.size.width + 3;
            auto textRect = context.setViewFrame(
                batteryTextView_, {{x, y}, {size.width, size.height}}, LayoutOptionsSizeToFit);
            x += textRect.size.width;
            return {x, std::max(iconRect.size.height, textRect.size.height)};
        });

    return size;
};

void PopupMenuScreen::drawInRect(Rect rect) {
    batteryIconView_.draw();
    batteryTextView_.draw();
    placeInReadableView_.draw();
    // For later :)
    // clockView_->draw();
}

void PopupMenuScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }
    if (!needsRerender) {
        return;
    }

    // Blank out screen
    display.fillScreen(display.bg);

    log_d("PopupMenuScreen render");

    batteryTextView_.setText(power_.getBatteryPercentageOnlyString());

    // Later :)
    // clockView_ = std::make_shared<SingleLineTextView>(display, "3:33pm");

    size_t place = renderer_.getCurrentPlace();
    size_t lengthOfFile = renderer_.size();

    float placeInTextPercentage = (float)place / (float)lengthOfFile * 100;
    std::string placeString = StringFormat("%.1f%%", placeInTextPercentage);
    std::string placeStringForBottom = StringFormat("%d / %d", place / 1000, lengthOfFile / 1000);

    placeInReadableView_.setText(placeStringForBottom);

    setNeedsLayout();

    setFrame({{0, 0}, {display.width(), display.height()}});

    display.setFont(display.style.getFontFamily().popupFont);
    draw();

    // Now draw!
    uint16_t textWidth = 0, textHeight = 0;
    auto batteryStatus = this->power_.getBatteryPercentageOnlyString();
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    // We want to square the items if we can.
    int yDirOffset = display.height() / 2 - display.width() / 2;
    if (yDirOffset < 0) {
        yDirOffset = 0;
    }
    int narrowToCenter = 20;

    std::string bottomStr =
        std::string("Seek ") + std::string("(") + placeString + std::string(")");

    display.getTextBounds(bottomStr.c_str(), &textWidth, &textHeight);
    display.drawSingleLineOfText(bottomStr, (screenWidth / 2) - (textWidth / 2),
                                 display.height() - yDirOffset - arrowWidth_ - arrowTextSpacing_ -
                                     narrowToCenter - textHeight);

    // Left Arrow
    display.drawLine(narrowToCenter, screenHeight / 2, arrowWidth_ + narrowToCenter,
                     screenHeight / 2 + arrowHeight_ / 2, display.fg);
    display.drawLine(narrowToCenter, screenHeight / 2, arrowWidth_ + narrowToCenter,
                     screenHeight / 2 - arrowHeight_ / 2, display.fg);

    // Down Arrow
    display.drawLine(display.width() / 2, display.height() - yDirOffset - narrowToCenter,
                     display.width() / 2 - arrowHeight_ / 2,
                     display.height() - yDirOffset - arrowWidth_ - narrowToCenter, display.fg);
    display.drawLine(display.width() / 2, display.height() - yDirOffset - narrowToCenter,
                     display.width() / 2 + arrowHeight_ / 2,
                     display.height() - yDirOffset - arrowWidth_ - narrowToCenter, display.fg);

    auto topStr = "Text";
    display.getTextBounds(topStr, &textWidth, &textHeight);
    display.drawSingleLineOfText(topStr, (screenWidth / 2) - (textWidth / 2),
                                 yDirOffset + arrowWidth_ + arrowTextSpacing_ + narrowToCenter);

    // Top Arrow
    display.drawLine(display.width() / 2, yDirOffset + narrowToCenter,
                     display.width() / 2 - arrowHeight_ / 2,
                     yDirOffset + arrowWidth_ + narrowToCenter, display.fg);
    display.drawLine(display.width() / 2, yDirOffset + narrowToCenter,
                     display.width() / 2 + arrowHeight_ / 2,
                     yDirOffset + arrowWidth_ + narrowToCenter, display.fg);

    display.getTextBounds(leftButtonTitle_.c_str(), &textWidth, &textHeight);
    display.drawSingleLineOfText(leftButtonTitle_.c_str(),
                                 arrowWidth_ + arrowTextSpacing_ + narrowToCenter,
                                 (screenHeight / 2) - textHeight / 2 - 1);

    // Right Arrow
    display.drawLine(display.width() - narrowToCenter, screenHeight / 2,
                     display.width() - arrowWidth_ - narrowToCenter,
                     screenHeight / 2 + arrowHeight_ / 2, display.fg);
    display.drawLine(display.width() - narrowToCenter, screenHeight / 2,
                     display.width() - arrowWidth_ - narrowToCenter,
                     screenHeight / 2 - arrowHeight_ / 2, display.fg);

    auto rightStr = "Dark Mode";
    display.getTextBounds(rightStr, &textWidth, &textHeight);
    display.drawSingleLineOfText(
        rightStr, (display.width() - arrowWidth_ - arrowTextSpacing_ - textWidth - narrowToCenter),
        (screenHeight / 2) - textHeight / 2 - 2);

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
