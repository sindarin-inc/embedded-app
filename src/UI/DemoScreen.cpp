#include "UI/DemoScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "UI/Bitmaps/arrow-up-down.hpp"
#include "UI/Bitmaps/book1.hpp"
#include "UI/Bitmaps/book2.hpp"
#include "UI/Bitmaps/book3.hpp"
#include "UI/Bitmaps/book4.hpp"
#include "UI/Bitmaps/bottom-arrow.hpp"
#include "UI/Bitmaps/eye1.hpp"
#include "UI/Bitmaps/eye2.hpp"
#include "UI/Bitmaps/eye3.hpp"
#include "UI/Bitmaps/glass-small-above.hpp"
#include "UI/Bitmaps/glass-small-left.hpp"
#include "UI/Bitmaps/glass-small-right.hpp"
#include "UI/Bitmaps/idea1.hpp"
#include "UI/Bitmaps/idea2.hpp"
#include "UI/Bitmaps/idea3.hpp"
#include "UI/Bitmaps/idea4.hpp"
#include "UI/Bitmaps/im10.hpp"
#include "UI/Bitmaps/im100.hpp"
#include "UI/Bitmaps/im20.hpp"
#include "UI/Bitmaps/im30.hpp"
#include "UI/Bitmaps/im40.hpp"
#include "UI/Bitmaps/im50.hpp"
#include "UI/Bitmaps/im60.hpp"
#include "UI/Bitmaps/im70.hpp"
#include "UI/Bitmaps/im80.hpp"
#include "UI/Bitmaps/im90.hpp"
#include "UI/Bitmaps/left-arrow.hpp"
#include "UI/Bitmaps/phone1.hpp"
#include "UI/Bitmaps/phone2.hpp"
#include "UI/Bitmaps/phone3.hpp"
#include "UI/Bitmaps/right-arrow.hpp"
#include "UI/Bitmaps/sol-reader1.hpp"
#include "UI/Bitmaps/sol-scroll-large.hpp"
#include "UI/Bitmaps/sol-scroll-small.hpp"
#include "UI/Bitmaps/sp1.hpp"
#include "UI/Bitmaps/sp2.hpp"
#include "UI/Bitmaps/sp3.hpp"
#include "UI/Bitmaps/sp4.hpp"
#include "UI/Bitmaps/space-scape1.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma20pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "UI/Layout/Types.hpp"
#include "sindarin-debug.h"

extern void UIInnerLoop(RemoteCommand command);

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs,
                       LightingController &lightingController)
    : display_(display), renderer_(display), prefs_(prefs),
      lightingController_(lightingController) {}

void DemoScreen::drawSkipMessage(DisplaySystem &display, bool inverted) {
    std::string message = SKIP_MESSAGE;
    display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(235), inverted);
}

void DemoScreen::drawDoubleLine(DisplaySystem &display, std::string one, std::string two) {
    std::string message;
    display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
    message = one;
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str()) / 2) -
            4);
    message = two;
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() + display.getTextHeight(message.c_str()) / 2));
}

void DemoScreen::drawTripleLine(DisplaySystem &display, std::string one, std::string two,
                                std::string three) {
    std::string message;
    display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
    message = one;
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str()) - 8));
    message = two;
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - 3));
    message = three;
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() + display.getTextHeight(message.c_str())));
}

void DemoScreen::render(DisplaySystem &display) {
    if (currentScreen_ > NUM_DEMO_SCREENS) {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
        // When we're done with the onboarding screen, turn this mode off.
        prefs_.demoMode.remove();
        back();
        return;
    }

    std::string message;
    auto nextUpdateType = DisplaySystem::UpdateType::FULL;
    auto nextInterval = 0;

    // Alternate left and right on for setup. Note: This seems backwards
    // but it looks right in-glass. Are LEFT and RIGHT backwards in the LightingController?
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
    int xOffset = -33;
    int yOffset = 53;

    int yOffsetOverall = -55;
    int textYOffset = 30;

    // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
    switch (currentScreen_) {
    case 0:
        switch (currentSubScreen_) {
        case 0:
            display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
            message = DEMO_SCREEN_1a;
            // This Y coordinate is just a guess
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = DEMO_SCREEN_1b;
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = DEMO_SCREEN_1c;
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 34));
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_left_width / 2),
                display.yAbsolute(128 - glass_small_left_height / 2 + yOffsetOverall),
                glass_small_left_bits, glass_small_left_width, glass_small_left_height, display.fg);
            display.setAltUpdateType(DisplaySystem::UpdateType::FAST, {{0, 0}, {255, 140}});
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false);
            display.setNoAltUpdateType();
            nextInterval = 500;
            break;
        case 1:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                left_arrow_bits, left_arrow_width, left_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 2:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                left_arrow_bits, left_arrow_width, left_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 3:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                right_arrow_bits, right_arrow_width, right_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 4:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                right_arrow_bits, right_arrow_width, right_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE);
            // wait 3 seconds before starting the animatiion over again.
            nextInterval = 2500;
            break;
        case 5:
            currentSubScreen_ = -1;
            nextInterval = 500;
            break;
        }
        break;
    case 1:
        xOffset = -xOffset;
        switch (currentSubScreen_) {
        case 0:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 - xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                right_arrow_bits, right_arrow_width, right_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 - xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                left_arrow_bits, left_arrow_width, left_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_left_width / 2),
                display.yAbsolute(128 - glass_small_left_height / 2 + yOffsetOverall),
                glass_small_left_bits, glass_small_left_width, glass_small_left_height, display.bg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE);
            display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
            message = DEMO_SCREEN_2a;
            // This Y coordinate is just a guess
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = DEMO_SCREEN_1b;
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = DEMO_SCREEN_1c;
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 34));
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_right_width / 2),
                display.yAbsolute(128 - glass_small_right_height / 2 + yOffsetOverall),
                glass_small_right_bits, glass_small_right_width, glass_small_right_height,
                display.fg);
            display.setAltUpdateType(DisplaySystem::UpdateType::FAST, {{0, 0}, {255, 140}});
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false);
            display.setNoAltUpdateType();
            nextInterval = 500;
            break;
        case 1:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                left_arrow_bits, left_arrow_width, left_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 2:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                left_arrow_bits, left_arrow_width, left_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 3:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                right_arrow_bits, right_arrow_width, right_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 4:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                right_arrow_bits, right_arrow_width, right_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, true);
            display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
            nextInterval = 2500;
            break;
        case 5:
            currentSubScreen_ = -1;
            nextInterval = 500;
            break;
        }
        break;
    case 2:
        xOffset = -2;
        yOffset = -40;
        switch (currentSubScreen_) {
        case 0:
            display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
            // fragile with the original xOffset above. Figure out a better way.
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 33),
                display.yAbsolute(128 - bottom_arrow_height / 2 + 53 + yOffsetOverall),
                bottom_arrow_bits, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + 33),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + 53 + yOffsetOverall),
                right_arrow_bits, right_arrow_width, right_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + 33),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + 53 + yOffsetOverall),
                left_arrow_bits, left_arrow_width, left_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_right_width / 2),
                display.yAbsolute(128 - glass_small_right_height / 2 + yOffsetOverall),
                glass_small_right_bits, glass_small_right_width, glass_small_right_height,
                display.bg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE);
            message = DEMO_SCREEN_3a;
            // This Y coordinate is just a guess
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = DEMO_SCREEN_3b;
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = DEMO_SCREEN_3c;
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 34));
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_above_width / 2),
                display.yAbsolute(128 - glass_small_above_height / 2 + yOffsetOverall),
                glass_small_above_bits, glass_small_above_width, glass_small_above_height,
                display.fg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.setAltUpdateType(DisplaySystem::UpdateType::FAST, {{0, 0}, {255, 140}});
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false);
            display.setNoAltUpdateType();
            nextInterval = 500;
            break;
        case 1:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.bg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 2:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.bg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 3:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.bg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval = 500;
            break;
        case 4:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                arrow_up_down_bits, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, true);
            nextInterval = 2500;
            break;
        case 5:
            currentSubScreen_ = -1;
            nextInterval = 500;
            break;
        }
        break;
    case 3:
        display.clearDisplay();
        display.clearFrameBuffer();
        display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        message = DEMO_SCREEN_4;
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str())));
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 4000;
        break;
    case 4:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), bk1_bits, bk1_width,
                            bk1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval = 2000;
        break;
    case 5:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), bk2_bits, bk2_width,
                            bk2_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval = 750;
        break;
    case 6:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), bk3_bits, bk3_width,
                            bk3_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval = 750;
        break;
    case 7:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), bk4_bits, bk4_width,
                            bk4_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval = 1000;
        break;
    case 8:
        display.clearDisplay();
        drawDoubleLine(display, DEMO_SCREEN_5a, DEMO_SCREEN_5b);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;
    case 9:
        drawDoubleLine(display, DEMO_SCREEN_6a, DEMO_SCREEN_6b);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;
    case 10:
        drawDoubleLine(display, DEMO_SCREEN_7a, DEMO_SCREEN_7b);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;
    case 11:
        drawDoubleLine(display, DEMO_SCREEN_8a, DEMO_SCREEN_8b);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;

    case 12:
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sp1_bits, sp1_width,
                            sp1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 2000;
        break;
    case 13:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sp2_bits, sp2_width,
                            sp2_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 14:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sp3_bits, sp3_width,
                            sp3_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 15:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sp4_bits, sp4_width,
                            sp4_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;
    case 16:
        drawDoubleLine(display, DEMO_SCREEN_9a, DEMO_SCREEN_9b);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;
    case 17:
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), idea1_bits, idea1_width,
                            idea1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 2000;
        break;
    case 18:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), idea2_bits, idea2_width,
                            idea2_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 19:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), idea3_bits, idea3_width,
                            idea3_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 20:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), idea4_bits, idea4_width,
                            idea4_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;
    case 21:
        display.invertDisplay(true);
        drawTripleLine(display, DEMO_SCREEN_10a, DEMO_SCREEN_10b, DEMO_SCREEN_10c);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;

    case 22:
        display.invertDisplay(false);
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), phone1_bits, phone1_width,
                            phone1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 2000;
        break;
    case 23:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), phone2_bits, phone2_width,
                            phone2_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 24:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), phone3_bits, phone3_width,
                            phone3_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;

    case 25:
        display.clearDisplay();
        drawTripleLine(display, DEMO_SCREEN_11a, DEMO_SCREEN_11b, DEMO_SCREEN_11c);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;
    case 26:
        display.clearDisplay();
        drawTripleLine(display, DEMO_SCREEN_12a, DEMO_SCREEN_12b, DEMO_SCREEN_12c);
        drawSkipMessage(display);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 3000;
        break;

    case 27:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), eye1_bits, eye1_width,
                            eye1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;
    case 28:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), eye2_bits, eye2_width,
                            eye2_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 29:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), eye3_bits, eye3_width,
                            eye3_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 750;
        break;
    case 30:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sol_reader1_bits,
                            sol_reader1_width, sol_reader1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;

    case 31:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;
    case 32:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im10_bits, im10_width, im10_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 33:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im20_bits, im20_width, im20_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 34:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im30_bits, im30_width, im30_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 35:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im40_bits, im40_width, im40_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 36:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im50_bits, im50_width, im50_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 37:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im60_bits, im60_width, im60_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 38:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im70_bits, im70_width, im70_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 39:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im80_bits, im80_width, im80_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 40:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            im90_bits, im90_width, im90_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 10;
        break;
    case 41:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), space_scape1_bits,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im100_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im100_height / 2),
                            im100_bits, im100_width, im100_height, display.bg);
        nextUpdateType = DisplaySystem::UpdateType::ERASE;
        nextInterval = 1500;
        break;
    case 42:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sol_scroll_small_bits,
                            sol_scroll_small_width, sol_scroll_small_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 750;
        break;
    case 43:
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), sol_scroll_large_bits,
                            sol_scroll_large_width, sol_scroll_large_height, display.fg);
        nextUpdateType = DisplaySystem::UpdateType::PARTIAL;
        nextInterval = 750;
        break;
    default:
        break;
    }

    if (currentScreen_ >= 3) {
        display.updateDisplay(nextUpdateType);
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval), 0);
        currentScreen_++;
    } else {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval), 0);
        currentSubScreen_++;
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
        if ((currentScreen_ > 0) && (currentScreen_ < 3)) {
            currentScreen_--;
        }
        currentSubScreen_ = 0;
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
        xTimerStop(DisplaySystem::animationTimer, 0);
        if (currentScreen_ >= 3) {
            display_.clearDisplay();
            currentScreen_ = NUM_DEMO_SCREENS;
        } else {
            currentScreen_++;
            currentSubScreen_ = 0;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::NEXT:
    case RemoteCommand::PREV:
    case RemoteCommand::BACK:
    default:
        break;
    }

    // For now, on the first three screens, we want to take the up button
    // and just advance the screen.
    if ((currentScreen_ < 3) &&
        ((command == RemoteCommand::BACK) || (command == RemoteCommand::PREV))) {
        currentScreen_++;
        currentSubScreen_ = 0;
        setNeedsRerender();
    }

    // Allow the user to press up to exit out of demo mode;
    if ((currentScreen_ >= NUM_DEMO_SCREENS) ||
        ((currentScreen_ >= 3) &&
         ((command == RemoteCommand::BACK) || command == RemoteCommand::PREV))) {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
        // When we're done with the onboarding screen, turn this mode off.
        prefs_.demoMode.remove();
        back();
    }
}
