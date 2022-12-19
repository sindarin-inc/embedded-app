#include "LightingAdjustScreen.hpp"

#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Fonts/LucidaGrande5pt7b.h"

#define LIGHTING_STEP_INCREMENT 10

void LightingAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    auto lightingPercent = lightingController_.getCurrentPercent();

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
    int16_t progressHeight = lightingPercent * Lightbulb_height / 100;
    display.fillRect(x, y + Lightbulb_height - progressHeight, barWidth, progressHeight,
                     display.fg);

    x += barWidth + spacing;
    display.setCursor(x, y + (Lightbulb_height + percentHeight) / 2);

    // Cursor is from the baseline of the text I think
    y += Lightbulb_height + display.getTextHeight() + spacing;
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

    if (lightUpMyLife_) {
        y += display.getTextHeight() + 10;
        auto message2 = "Let there be light!";
        display.getTextBounds(message2, &textWidth, &textHeight);
        display.setCursor((display.width() - textWidth) / 2, y);
        display.println(message2);
    }

    // up arrow
    uint16_t arrowWidth = 5, arrowHeight = 10;
    x = display.width() - arrowWidth;
    y = display.height() / 2;
    display.drawLine(x, y, x - arrowWidth, y - arrowHeight / 2, display.fg);
    display.drawLine(x, y, x - arrowWidth, y + arrowHeight / 2, display.fg);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;

    needsRerender = false;
}

void LightingAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        lightUpMyLife_ = true;
        firstLoad = true;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        lightUpMyLife_ = false;
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    case RemoteCommand::PREV: {
        auto setting =
            std::min(lightingController_.getCurrentPercent() + LIGHTING_STEP_INCREMENT, 100);
        lightingController_.setOverrideValuePercent(setting);
        needsRerender = true;
        break;
    }
    case RemoteCommand::NEXT: {
        auto setting =
            std::max(lightingController_.getCurrentPercent() - LIGHTING_STEP_INCREMENT, 0);
        lightingController_.setOverrideValuePercent(setting);
        needsRerender = true;
        break;
    }
    case RemoteCommand::BACK:
        lightUpMyLife_ = false;
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
