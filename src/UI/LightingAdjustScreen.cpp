#include "LightingAdjustScreen.hpp"

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Fonts/LucidaGrande5pt7b.h"

#define MIN_LIGHTING_VALUE 10
#define MAX_LIGHTING_VALUE 30
#define LIGHTING_STEP_INCREMENT 1

void LightingAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    // TODO: Kill this
    auto autoUpdate = display.getAutoUpdate();
    display.setAutoUpdate(false);

    auto backlightAbsolute = prefs_.backlight.get().value();
    auto backlightPercent =
        (backlightAbsolute - MIN_LIGHTING_VALUE) * 100 / (MAX_LIGHTING_VALUE - MIN_LIGHTING_VALUE);

    uint16_t percentWidth, percentHeight;
    // Just pick a number for measurement. We don't want the UI moving when the number changes
    display.getTextBounds("50%", &percentWidth, &percentHeight);

    int16_t spacing = 8;
    int16_t barWidth = 10;
    int16_t x = (display.width() - Lightbulb_width - 2 * spacing - percentWidth - barWidth) / 2;
    // 98 is a pre-calculated height.
    // TODO: Do this calculation dynamically, once we have a way to gracefully layout subviews
    int16_t y = (display.height() - 98) / 2;

    display.drawXBitmap(x, y, lightbulbBits, Lightbulb_width, Lightbulb_height, display.fg);

    x += Lightbulb_width + spacing;
    display.drawRect(x, y, barWidth, Lightbulb_height, display.fg);
    int16_t progressHeight = backlightPercent * Lightbulb_height / 100;
    display.fillRect(x, y + Lightbulb_height - progressHeight, barWidth, progressHeight,
                     display.fg);

    x += barWidth + spacing;
    display.setCursor(x, y + (Lightbulb_height + percentHeight) / 2);
    // Not drawing for some reason hmm
    display.println(StringFormat("%d%%", backlightPercent).c_str());

    // Cursor is from the baseline of the text I think
    y += Lightbulb_height + display.getTextHeight() + spacing;
    x = 0;
    uint16_t textWidth, textHeight;
    auto message1 = "Press up and down";
    display.getTextBounds(message1, &textWidth, &textHeight);
    display.setCursor((display.width() - textWidth) / 2, y);
    display.println(message1);

    y += display.getTextHeight();
    auto message2 = "to adjust brightness";
    display.getTextBounds(message2, &textWidth, &textHeight);
    display.setCursor((display.width() - textWidth) / 2, y);
    display.println(message2);

    // Right arrow
    uint16_t arrowWidth = 7, arrowHeight = 36;
    x = display.width();
    y = (display.height() - arrowHeight) / 2;
    display.drawLine(x, y, x + arrowWidth, y + arrowHeight / 2, display.fg);
    display.drawLine(x + arrowWidth, y + arrowHeight / 2, x, y + arrowHeight, display.fg);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;
    display.setAutoUpdate(autoUpdate);

    needsRerender = false;
}

void LightingAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        break;
    case RemoteCommand::CHANNEL_PLUS:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    case RemoteCommand::PREV: {
        auto backlight = prefs_.backlight.get().value();
        backlight = std::min(backlight + LIGHTING_STEP_INCREMENT, MAX_LIGHTING_VALUE);
        SetBacklight(backlight);
        prefs_.backlight.set(backlight);
        needsRerender = true;
        break;
    }
    case RemoteCommand::NEXT: {
        auto backlight = prefs_.backlight.get().value();
        backlight = std::max(backlight - LIGHTING_STEP_INCREMENT, MIN_LIGHTING_VALUE);
        SetBacklight(backlight);
        prefs_.backlight.set(backlight);
        needsRerender = true;
        break;
    }
    case RemoteCommand::BACK:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
