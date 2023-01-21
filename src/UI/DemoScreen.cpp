#include "UI/DemoScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "UI/Bitmaps/Logo.hpp"
#include "UI/Bitmaps/Sol_Logo.hpp"
#include "UI/Bitmaps/stone.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma20pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "sindarin-debug.h"

extern void UIInnerLoop(RemoteCommand command);

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power,
                       LightingController &lightingController)
    : display_(display), renderer_(display), prefs_(prefs), power_(power),
      lightingController_(lightingController) {}

void DemoScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    std::string message = messages_[currentScreen_];

    display.setFont(&TAHOMA_REGULAR_FONT6PT7B);

    // Alternate left and right on for setup.
    if (currentScreen_ == 1) {
        lightingController_.setScreenMask(LightingController::Screens::LEFT);
    } else if (currentScreen_ == 0) {
        lightingController_.setScreenMask(LightingController::Screens::RIGHT);
    }

    // Turn on both screen lights for last case
    if (currentScreen_ >= 2) {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
    }
    // Don't need to set it after that as they are on.

    // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
    switch (currentScreen_) {
    case 0:
    case 1:
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
        break;
    case 2:
    case 3:
        display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        break;
    }
    // NOLINTEND(bugprone-branch-clone)

    if (currentScreen_ == 3) {
        message = DEMO_SCREEN_4;

        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str())));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(2500);
        display.clearDisplay();

        message = DEMO_SCREEN_5;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str())));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(2000);

        message = DEMO_SCREEN_6;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         7 * display.getTextHeight(message.c_str()));
        message = DEMO_SCREEN_6a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         5.5 * display.getTextHeight(message.c_str()));

        display.drawCircle(display.width() / 2, 100, 25, display.fg);
        display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
        display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

        display.drawLine(display.width() / 2 - 8, 101, display.width() / 2 + 12, 101, display.fg);

        display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(250);

        message = DEMO_SCREEN_6;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         7 * display.getTextHeight(message.c_str()));
        message = DEMO_SCREEN_6a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         5.5 * display.getTextHeight(message.c_str()));

        display.drawCircle(display.width() / 2, 100, 25, display.fg);
        display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
        display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

        display.drawLine(display.width() / 2 - 8, 101, display.width() / 2 + 12, 101, display.fg);

        display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108, display.fg);

        display.drawLine(display.width() / 2 - 12, 115, display.width() / 2 + 12, 115, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::FAST);
        delay(750);

        message = DEMO_SCREEN_6;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         7 * display.getTextHeight(message.c_str()));
        message = DEMO_SCREEN_6a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         5.5 * display.getTextHeight(message.c_str()));

        display.drawCircle(display.width() / 2, 100, 25, display.fg);
        display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
        display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

        display.drawLine(display.width() / 2 - 8, 101, display.width() / 2 + 12, 101, display.fg);

        display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108, display.fg);

        display.drawLine(display.width() / 2 - 12, 115, display.width() / 2 + 12, 115, display.fg);

        display.drawLine(display.width() / 2 - 12, 122, display.width() / 2 + 12, 122, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::FAST);
        delay(750);

        message = DEMO_SCREEN_6;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         7 * display.getTextHeight(message.c_str()));
        message = DEMO_SCREEN_6a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         5.5 * display.getTextHeight(message.c_str()));

        display.drawCircle(display.width() / 2, 100, 25, display.fg);
        display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
        display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

        display.drawLine(display.width() / 2 - 8, 101, display.width() / 2 + 12, 101, display.fg);

        display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108, display.fg);

        display.drawLine(display.width() / 2 - 12, 115, display.width() / 2 + 12, 115, display.fg);

        display.drawLine(display.width() / 2 - 12, 122, display.width() / 2 + 12, 122, display.fg);

        display.drawLine(display.width() / 2 - 12, 129, display.width() / 2 + 8, 129, display.fg);
        display.updateDisplay(DisplaySystem::UpdateType::FAST);

        display.clearDisplay();
        delay(750);

        message = DEMO_SCREEN_7;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(1500);

        message = DEMO_SCREEN_8;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         4.5 * display.getTextHeight(message.c_str()));

        message = DEMO_SCREEN_8a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         3 * display.getTextHeight(message.c_str()));
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - papyrus2_width / 2), 90,
                            papyrus2Bits, papyrus2_width, papyrus2_height, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::FULL);
        delay(3000);

        message = DEMO_SCREEN_9;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(1500);

        message = DEMO_SCREEN_10;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2) -
                                         6);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - paper_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - paper_height / 2),
                            paperBits, paper_width, paper_height, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(1500);

        message = DEMO_SCREEN_11;

        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2 +
                                                       35),
                                     display.yAbsolute(58 + hi_hand_height / 2 -
                                                       display.getTextHeight(message.c_str()) / 2));

        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45,
                            30, hiHandBits, hi_hand_width, hi_hand_height, display.fg);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
        delay(1500);
        message = DEMO_SCREEN_11;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2 +
                                                       35),
                                     display.yAbsolute(58 + hi_hand_height / 2 -
                                                       display.getTextHeight(message.c_str()) / 2));
        message = DEMO_SCREEN_11a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2 +
                                                       35),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2));

        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45,
                            30, hiHandBits, hi_hand_width, hi_hand_height, display.fg);

        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_blocks_width / 2) - 45,
                            display.yAbsolute(display.yAbsoluteCenter() - hi_blocks_height / 2),
                            hiBlocksBits, hi_blocks_width, hi_blocks_height, display.fg);
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(1500);

        message = DEMO_SCREEN_11;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2 +
                                                       35),
                                     display.yAbsolute(58 + hi_hand_height / 2 -
                                                       display.getTextHeight(message.c_str()) / 2));
        message = DEMO_SCREEN_11a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2 +
                                                       35),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2));
        message = DEMO_SCREEN_11b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2 +
                                                       35),
                                     display.yAbsolute(display.yAbsoluteCenter() + 45));
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45,
                            30, hiHandBits, hi_hand_width, hi_hand_height, display.fg);

        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_blocks_width / 2) - 45,
                            display.yAbsolute(display.yAbsoluteCenter() - hi_blocks_height / 2),
                            hiBlocksBits, hi_blocks_width, hi_blocks_height, display.fg);

        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_pixel3_width / 2) - 45,
                            130, hiPixel1Bits, hi_pixel1_width, hi_pixel1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - hi_pixel3_width / 2) - 45,
                            130, hiPixel3Bits, hi_pixel3_width, hi_pixel3_height, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        display.clearDisplay();

        delay(1500);
        message = DEMO_SCREEN_12;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(1500);

        message = DEMO_SCREEN_13;
        display.drawSingleLineOfText(
            message.c_str(), display.xAbsolute(display.xAbsoluteCenter() - 65),
            display.yAbsolute(display.yAbsoluteCenter() -
                              display.getTextHeight(message.c_str()) / 2 -
                              1.5 * display.getTextHeight(message.c_str())));

        message = DEMO_SCREEN_13a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       0 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_13b;
        display.drawSingleLineOfText(
            message.c_str(), display.xAbsolute(display.xAbsoluteCenter() - 65),
            display.yAbsolute(display.yAbsoluteCenter() -
                              display.getTextHeight(message.c_str()) / 2 +
                              1.5 * display.getTextHeight(message.c_str())));
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - spark_width / 2) + 55, 45,
                            sparkBits, spark_width, spark_height, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(4000);

        message = DEMO_SCREEN_14;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       3 * display.getTextHeight(message.c_str())));

        message = DEMO_SCREEN_14a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       2 * display.getTextHeight(message.c_str())));

        message = DEMO_SCREEN_14b;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() -
                              display.getTextHeight(message.c_str()) / 2 +
                              1.5 * display.getTextHeight(message.c_str())));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(4000);

        message = DEMO_SCREEN_15;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2));

        display.clearDisplay();

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(2000);

        message = DEMO_SCREEN_16;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() - 30));
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
        delay(1000);

        message = DEMO_SCREEN_16;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() - 30));
        message = DEMO_SCREEN_16a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 35),
                                     display.yAbsolute(display.yAbsoluteCenter()));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(1000);

        message = DEMO_SCREEN_16;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() - 30));
        message = DEMO_SCREEN_16a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 35),
                                     display.yAbsolute(display.yAbsoluteCenter()));

        message = DEMO_SCREEN_16b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter()) - 10,
                                     display.yAbsolute(display.yAbsoluteCenter() + 25));

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(100);

        message = DEMO_SCREEN_16;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() - 30));
        message = DEMO_SCREEN_16a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 35),
                                     display.yAbsolute(display.yAbsoluteCenter()));

        message = DEMO_SCREEN_16b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter()) - 10,
                                     display.yAbsolute(display.yAbsoluteCenter() + 25));

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3, display.fg);

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 2, 125 - 2, 4,
                           display.fg);
        display.updateDisplay(DisplaySystem::UpdateType::FAST);
        delay(100);

        message = DEMO_SCREEN_16;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() - 30));
        message = DEMO_SCREEN_16a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 35),
                                     display.yAbsolute(display.yAbsoluteCenter()));

        message = DEMO_SCREEN_16b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter()) - 10,
                                     display.yAbsolute(display.yAbsoluteCenter() + 25));

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3, display.fg);

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 2, 125 - 2, 4,
                           display.fg);
        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 10, 125 - 11, 5,
                           display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::FAST);
        delay(100);

        message = DEMO_SCREEN_16;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 65),
                                     display.yAbsolute(display.yAbsoluteCenter() - 30));
        message = DEMO_SCREEN_16a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() - 35),
                                     display.yAbsolute(display.yAbsoluteCenter()));

        message = DEMO_SCREEN_16b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter()) - 10,
                                     display.yAbsolute(display.yAbsoluteCenter() + 25));

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3, display.fg);

        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 2, 125 - 2, 4,
                           display.fg);
        display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 10, 125 - 11, 5,
                           display.fg);

        display.drawRoundRect(display.xAbsoluteCenter() - thought_width / 2 + 12,
                              125 - 14 - 7 - 3 - 41, 65, 50, 12, display.fg);
        message = DEMO_SCREEN_16e;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2 + 59),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 - 3));
        message = DEMO_SCREEN_16d;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2 + 59),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 - 3 -
                              display.getTextHeight()));

        message = DEMO_SCREEN_16c;

        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2 + 59),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 - 3 -
                              display.getTextHeight() * 2));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        display.clearDisplay();
        delay(2500);

        message = DEMO_SCREEN_17;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       5 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_17a;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() -
                              display.getTextHeight(message.c_str()) / 2 -
                              3.5 * display.getTextHeight(message.c_str())));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(1500);

        message = DEMO_SCREEN_17;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       5 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_17a;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() -
                              display.getTextHeight(message.c_str()) / 2 -
                              3.5 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_17b;

        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2) +
                                         2,
                                     display.yAbsolute(display.yAbsoluteCenter() + 10));
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - lightbulb_width / 2), 90,
                            lightbulbBits, lightbulb_width, lightbulb_height, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(1500);

        message = DEMO_SCREEN_18;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       8 * display.getTextHeight(message.c_str())));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(750);

        message = DEMO_SCREEN_18;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       8 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_18a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2) +
                                         30,
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       2 * display.getTextHeight(message.c_str())));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(500);

        message = DEMO_SCREEN_18;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       8 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_18a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2) +
                                         30,
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       2 * display.getTextHeight(message.c_str())));

        message = DEMO_SCREEN_18b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2) -
                                         30,
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 +
                                                       1 * display.getTextHeight(message.c_str())));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(500);

        message = DEMO_SCREEN_18;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       8 * display.getTextHeight(message.c_str())));
        message = DEMO_SCREEN_18a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2) +
                                         30,
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 -
                                                       2 * display.getTextHeight(message.c_str())));

        message = DEMO_SCREEN_18b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2) -
                                         30,
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 +
                                                       1 * display.getTextHeight(message.c_str())));

        message = DEMO_SCREEN_18c;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight(message.c_str()) / 2 +
                                                       4 * display.getTextHeight(message.c_str())));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(1500);
        display.clearDisplay();

        message = DEMO_SCREEN_19;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 -
                                                       5 * display.getTextHeight()));
        message = DEMO_SCREEN_19a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 -
                                                       3.5 * display.getTextHeight()));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(2000);
        message = DEMO_SCREEN_19;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 -
                                                       5 * display.getTextHeight()));
        message = DEMO_SCREEN_19a;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 -
                                                       3.5 * display.getTextHeight()));
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - franklin_width / 2), 80,
                            franklinBits, franklin_width, franklin_height, display.fg);
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        delay(2250);

        message = DEMO_SCREEN_20;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 -
                                                       1.5 * display.getTextHeight()));

        message = DEMO_SCREEN_20a;

        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 -
                                                       0 * display.getTextHeight()));
        message = DEMO_SCREEN_20b;
        display.drawSingleLineOfText(message.c_str(),
                                     display.xAbsolute(display.xAbsoluteCenter() -
                                                       display.getTextWidth(message.c_str()) / 2),
                                     display.yAbsolute(display.yAbsoluteCenter() -
                                                       display.getTextHeight() / 2 +
                                                       1.5 * display.getTextHeight()));

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(5000);
        display.drawXBitmap(display.width() / 2 - sol_logo_width / 2,
                            display.height() / 2 - sol_logo_height / 2, solLogoBits, sol_logo_width,
                            sol_logo_height, display.fg);
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        delay(3000);
    } else {
        // This Y coordinate is just a guess
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2));
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    needsRerender = false;

    if (currentScreen_ == 3) {
        RemoteCommand toSend = RemoteCommand::NEXT;
        DispatchGetUiQueue()->dispatch([toSend]() { UIInnerLoop(toSend); });
    }
}

void DemoScreen::processCommand(RemoteCommand command) {
    // Note that this gets called even when there isn't a button press
    // if the screen is set from RootUI as the first screen to be shown.
    // That's why the increment is in the case statement instead of
    // below.
    //
    // Eventually, we'll add more logic to do the right thing based on
    // the messages, but for now, any button press is fine.
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::PREV:
    case RemoteCommand::NEXT:
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
        currentScreen_++;
        needsRerender = true;
        break;
    default:
        break;
    }

    if (currentScreen_ >= NUM_DEMO_SCREENS) {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
        // When we're done with the onboarding screen, turn this mode off.
        prefs_.demoMode.set(0);
        backCallback();
    }
}
