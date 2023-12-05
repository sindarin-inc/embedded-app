#include "UI/DemoScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Communications/WiFi.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "UI/Bitmaps/arrow-up-down.hpp"
#include "UI/Bitmaps/bottom-arrow.hpp"
#include "UI/Bitmaps/glass-small-above.hpp"
#include "UI/Bitmaps/glass-small-left.hpp"
#include "UI/Bitmaps/glass-small-right.hpp"
#include "UI/Bitmaps/left-arrow.hpp"
#include "UI/Bitmaps/mani-astronaut.hpp"
#include "UI/Bitmaps/mani-book1.hpp"
#include "UI/Bitmaps/mani-book2.hpp"
#include "UI/Bitmaps/mani-book3.hpp"
#include "UI/Bitmaps/mani-eye1.hpp"
#include "UI/Bitmaps/mani-eye2.hpp"
#include "UI/Bitmaps/mani-eye3.hpp"
#include "UI/Bitmaps/mani-eye4.hpp"
#include "UI/Bitmaps/mani-eye5.hpp"
#include "UI/Bitmaps/mani-eye6.hpp"
#include "UI/Bitmaps/mani-eye7.hpp"
#include "UI/Bitmaps/mani-eye8.hpp"
#include "UI/Bitmaps/mani-eye9.hpp"
#include "UI/Bitmaps/mani-phone1.hpp"
#include "UI/Bitmaps/mani-phone2.hpp"
#include "UI/Bitmaps/mani-phone3.hpp"
#include "UI/Bitmaps/mani-phone4.hpp"
#include "UI/Bitmaps/mani-phone5.hpp"
#include "UI/Bitmaps/mani-ship.hpp"
#include "UI/Bitmaps/mani-spark1.hpp"
#include "UI/Bitmaps/mani-spark2.hpp"
#include "UI/Bitmaps/mani-spark3.hpp"
#include "UI/Bitmaps/mani-spark4.hpp"
#include "UI/Bitmaps/mani-spark5.hpp"
#include "UI/Bitmaps/mani-turnpage-large-a.hpp"
#include "UI/Bitmaps/mani-turnpage-large-b.hpp"
#include "UI/Bitmaps/mani-turnpage-small.hpp"
#include "UI/Bitmaps/mani-volcano.hpp"
#include "UI/Bitmaps/mani-warrior.hpp"
#include "UI/Bitmaps/right-arrow.hpp"
#include "UI/Bitmaps/sol-logo.hpp"
#include "UI/Layout/Types.hpp"
#include "sindarin-debug.h"

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs,
                       LightingController &lightingController)
    : display_(display), renderer_(display), prefs_(prefs),
      lightingController_(lightingController) {
    // We want to control exactly when full and partial will happen.
    // This allows us to cross ownership boundaries and to force a partial update
    // even when we switch to the main menu the first time.
    display.forceDontChangeRefreshType();
    lightingController_.enableOnlyIncreaseMode();

    lambdas_.emplace_back([this](DisplaySystem &display) {
        // Don't need to set it after that as they are on.
        int xOffset = -33;
        int yOffset = 53;

        int yOffsetOverall = -55;
        int textYOffset = 30;

        std::string message;

        switch (currentSubScreen_) {
        case 0:
            display.setFont(static_cast<Font *>(&ibmf75Face1));
            message = "Twist lens in front of LEFT eye";
            // This Y coordinate is just a guess
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = "like a binocular. When text is";
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = "crisp, press \ue000 on your remote.";
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 34));
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_left_width / 2),
                display.yAbsolute(128 - glass_small_left_height / 2 + yOffsetOverall),
                GLASS_SMALL_LEFT_BITS, glass_small_left_width, glass_small_left_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false, 0,
                                  DisplaySystem::UpdateType::FAST, {{0, 0}, {255, 140}});
            nextInterval_ = 500;
            break;
        case 1:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                LEFT_ARROW_BITS, left_arrow_width, left_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 2:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                LEFT_ARROW_BITS, left_arrow_width, left_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 3:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                RIGHT_ARROW_BITS, right_arrow_width, right_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 4:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                RIGHT_ARROW_BITS, right_arrow_width, right_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            // wait 3 seconds before starting the animatiion over again.
            nextInterval_ = 2500;
            break;
        case 5:
            currentSubScreen_ = 0;
            nextInterval_ = 500;
            break;
        }
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        // Don't need to set it after that as they are on.
        int xOffset = 33;
        int yOffset = 53;

        int yOffsetOverall = -55;
        int textYOffset = 30;

        std::string message;
        switch (currentSubScreen_) {
        case 0:
            if (!shownOne_) {
                display.drawXBitmap(
                    display.xAbsolute(128 - bottom_arrow_width / 2 - xOffset),
                    display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                    BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.bg);
                display.drawXBitmap(display.xAbsolute(128 - bottom_arrow_width / 2 + 14 - xOffset),
                                    display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset +
                                                      yOffsetOverall),
                                    RIGHT_ARROW_BITS, right_arrow_width, right_arrow_height,
                                    display.bg);
                display.drawXBitmap(display.xAbsolute(128 - bottom_arrow_width / 2 - 14 - xOffset),
                                    display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset +
                                                      yOffsetOverall),
                                    LEFT_ARROW_BITS, left_arrow_width, left_arrow_height,
                                    display.bg);
                display.drawXBitmap(
                    display.xAbsolute(128 - glass_small_left_width / 2),
                    display.yAbsolute(128 - glass_small_left_height / 2 + yOffsetOverall),
                    GLASS_SMALL_LEFT_BITS, glass_small_left_width, glass_small_left_height,
                    display.bg);
                display.updateDisplay(DisplaySystem::UpdateType::ERASE);
                shownOne_ = true;
            }
            display.setFont(static_cast<Font *>(&ibmf75Face1));
            message = "Twist lens in front of RIGHT eye";
            // This Y coordinate is just a guess
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = "like a binocular. When text is";
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = "crisp, press \ue000 on your remote.";
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 34));
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_right_width / 2),
                display.yAbsolute(128 - glass_small_right_height / 2 + yOffsetOverall),
                GLASS_SMALL_RIGHT_BITS, glass_small_right_width, glass_small_right_height,
                display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false, 0,
                                  DisplaySystem::UpdateType::FAST, {{0, 0}, {255, 140}});
            nextInterval_ = 500;
            break;
        case 1:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                LEFT_ARROW_BITS, left_arrow_width, left_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 2:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                LEFT_ARROW_BITS, left_arrow_width, left_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 3:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                RIGHT_ARROW_BITS, right_arrow_width, right_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 4:
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + yOffset + yOffsetOverall),
                RIGHT_ARROW_BITS, right_arrow_width, right_arrow_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - bottom_arrow_width / 2 + xOffset),
                display.yAbsolute(128 - bottom_arrow_height / 2 + yOffset + yOffsetOverall),
                BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            display.setFont(static_cast<Font *>(&ibmf75Face1));
            nextInterval_ = 2500;
            break;
        case 5:
            currentSubScreen_ = 0;
            nextInterval_ = 500;
            break;
        }
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        int xOffset = -2;
        int yOffset = -40;

        int yOffsetOverall = -55;
        int textYOffset = 30;

        std::string message;

        switch (currentSubScreen_) {
        case 0:
            display.setFont(static_cast<Font *>(&ibmf75Face1));
            if (!shownTwo_) {
                // fragile with the original xOffset above. Figure out a better way.
                display.drawXBitmap(
                    display.xAbsolute(128 - bottom_arrow_width / 2 + 33),
                    display.yAbsolute(128 - bottom_arrow_height / 2 + 53 + yOffsetOverall),
                    BOTTOM_ARROW_BITS, bottom_arrow_width, bottom_arrow_height, display.bg);
                display.drawXBitmap(
                    display.xAbsolute(128 - bottom_arrow_width / 2 + 14 + 33),
                    display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + 53 + yOffsetOverall),
                    RIGHT_ARROW_BITS, right_arrow_width, right_arrow_height, display.bg);
                display.drawXBitmap(
                    display.xAbsolute(128 - bottom_arrow_width / 2 - 14 + 33),
                    display.yAbsolute(128 - bottom_arrow_height / 2 - 22 + 53 + yOffsetOverall),
                    LEFT_ARROW_BITS, left_arrow_width, left_arrow_height, display.bg);
                display.drawXBitmap(
                    display.xAbsolute(128 - glass_small_right_width / 2),
                    display.yAbsolute(128 - glass_small_right_height / 2 + yOffsetOverall),
                    GLASS_SMALL_RIGHT_BITS, glass_small_right_width, glass_small_right_height,
                    display.bg);
                display.updateDisplay(DisplaySystem::UpdateType::ERASE);
                shownTwo_ = true;
            }
            message = "Optionally, turn the knob on TOP ";
            // This Y coordinate is just a guess
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = "to adjust lens spacing. When ";
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = "text is comfortable, press \ue000";
            display.drawSingleLineOfText(
                message,
                display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 34));
            display.drawXBitmap(
                display.xAbsolute(128 - glass_small_above_width / 2),
                display.yAbsolute(128 - glass_small_above_height / 2 + yOffsetOverall),
                GLASS_SMALL_ABOVE_BITS, glass_small_above_width, glass_small_above_height,
                display.fg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false, 0,
                                  DisplaySystem::UpdateType::FAST, {{0, 0}, {255, 140}});
            nextInterval_ = 500;
            break;
        case 1:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.bg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 2:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.bg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 3:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.bg);

            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 500;
            break;
        case 4:
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall - 12),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.bg);
            display.drawXBitmap(
                display.xAbsolute(128 - arrow_up_down_width / 2 + xOffset),
                display.yAbsolute(128 - arrow_up_down_height / 2 + yOffset + yOffsetOverall),
                ARROW_UP_DOWN_BITS, arrow_up_down_width, arrow_up_down_height, display.fg);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false);
            nextInterval_ = 2500;
            break;
        case 5:
            currentSubScreen_ = 0;
            nextInterval_ = 500;
            break;
        }
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        std::string message = "Welcome to the Sol Reader! Let's begin...";
        display.clearDisplay();
        display.clearFrameBuffer();
        display.setFont(static_cast<Font *>(&ibmf75Face0));
        display.drawSingleLineOfText(
            message,
            display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message)));
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 3000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_BOOK1_BITS,
                            mani_book1_width, mani_book1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1250;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_BOOK2_BITS,
                            mani_book2_width, mani_book2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_BOOK3_BITS,
                            mani_book3_width, mani_book3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_BOOK3_BITS,
                            mani_book3_width, mani_book3_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = false;
        display.clearDisplay();
        drawDoubleLine(display, "The way we read  ", "is always changing.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawDoubleLine(display, "But across the centuries", "one thing hasn't.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawDoubleLine(display, "What reading does", "for our minds.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawDoubleLine(display, "Whisking us away", "into other realms.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_TURNPAGE_SMALL_BITS,
                            mani_turnpage_small_width, mani_turnpage_small_height, display.fg);
        drawDoubleLine(display, "Whisking us away", "into other realms.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(
            display.xAbsolute(display.xAbsoluteMax() - mani_turnpage_large_a_width + 1),
            display.yAbsolute(display.yAbsoluteMax() - mani_turnpage_large_a_height + 1),
            MANI_TURNPAGE_LARGE_A_BITS, mani_turnpage_large_a_width, mani_turnpage_large_a_height,
            display.fg);
        display.drawXBitmap(
            display.xAbsolute(display.xAbsoluteMax() - mani_turnpage_large_b_width + 1),
            display.yAbsolute(display.yAbsoluteMax() - mani_turnpage_large_a_height -
                              mani_turnpage_large_b_height + 1),
            MANI_TURNPAGE_LARGE_B_BITS, mani_turnpage_large_b_width, mani_turnpage_large_b_height,
            display.fg);
        drawDoubleLine(display, "Whisking us away", "into other realms.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_VOLCANO_BITS,
                            mani_volcano_width, mani_volcano_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_ASTRONAUT_BITS,
                            mani_astronaut_width, mani_astronaut_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_SHIP_BITS,
                            mani_ship_width, mani_ship_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_WARRIOR_BITS,
                            mani_warrior_width, mani_warrior_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 1250;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = false;
        drawDoubleLine(display, "Sparking new", "thoughts and ideas.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_SPARK1_BITS,
                            mani_spark1_width, mani_spark1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_SPARK2_BITS,
                            mani_spark2_width, mani_spark2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_SPARK3_BITS,
                            mani_spark3_width, mani_spark3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_SPARK4_BITS,
                            mani_spark4_width, mani_spark4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_SPARK5_BITS,
                            mani_spark5_width, mani_spark5_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = false;
        display.invertDisplay(true);
        drawTripleLine(display, "In this era of", "constant beeps,", "alerts, and updates...");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.invertDisplay(false);
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_PHONE1_BITS,
                            mani_phone1_width, mani_phone1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_PHONE2_BITS,
                            mani_phone2_width, mani_phone2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_PHONE3_BITS,
                            mani_phone3_width, mani_phone3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_PHONE4_BITS,
                            mani_phone4_width, mani_phone4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_PHONE5_BITS,
                            mani_phone5_width, mani_phone5_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = false;
        display.invertDisplay(true);
        drawTripleLine(display, "...what will the", "next evolution of", "reading look like?");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 2250;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawTripleLine(display, "Simply lean back,", "open your eyes, and",
                       "let words light the way.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 3000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawTripleLine(display, "Simply lean back,", "open your eyes, and",
                       "let words light the way.", true);
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.invertDisplay(false);
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE1_BITS,
                            mani_eye1_width, mani_eye1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1250;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(false);
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE2_BITS,
                            mani_eye2_width, mani_eye2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE3_BITS,
                            mani_eye3_width, mani_eye3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE4_BITS,
                            mani_eye4_width, mani_eye4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE5_BITS,
                            mani_eye5_width, mani_eye5_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE6_BITS,
                            mani_eye6_width, mani_eye6_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE7_BITS,
                            mani_eye7_width, mani_eye7_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE8_BITS,
                            mani_eye8_width, mani_eye8_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), MANI_EYE9_BITS,
                            mani_eye9_width, mani_eye9_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = false;
        display.invertDisplay(false);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - sol_logo_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - sol_logo_height / 2),
                            SOL_LOGO_BITS, sol_logo_width, sol_logo_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 2500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = false;
        display.invertDisplay(false);
        drawTripleLine(display, "Download the Sol App", "to get started:", "solreader.com/app");
        std::string message = "Press \ue000 after app setup";
        prefs_.noSetupMode.set(true);
        display.setFont(static_cast<Font *>(&ibmf75Face0));
        display.drawSingleLineOfText(
            message,
            display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
            display.yAbsolute(235));
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
    });
}

void DemoScreen::drawSerial(DisplaySystem &display) {
    // Show serial number in tiny text in the bottom
    display.setFont(static_cast<Font *>(&ibmf75Face1));
    std::string message = "SN: " + GetSerialNumber();
    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(display.yAbsoluteMax() - 2 * display.getTextHeight(message)));
}

void DemoScreen::drawSkipMessage(DisplaySystem &display, bool inverted) {
    std::string message = "Press \ue000 to skip";
    display.setFont(static_cast<Font *>(&ibmf75Face0));
    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(235), inverted);
}

void DemoScreen::drawDoubleLine(DisplaySystem &display, std::string one, std::string two) {
    std::string message;
    display.setFont(static_cast<Font *>(&ibmf75Face1));
    message = std::move(one);
    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message) / 2) - 12);
    message = std::move(two);
    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(display.yAbsoluteCenter() + display.getTextHeight(message) / 2 - 4));
}

void DemoScreen::drawTripleLine(DisplaySystem &display, std::string one, std::string two,
                                std::string three, bool invert) {
    std::string message;
    display.setFont(static_cast<Font *>(&ibmf75Face1));
    message = std::move(one);
    int yAdjust = display.getTextHeight() - display.getLineSpacing();

    yAdjust = (yAdjust + 1) / 2;

    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - yAdjust - yAdjust * 2 - 4), invert);
    message = std::move(two);
    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - yAdjust), invert);
    message = std::move(three);
    display.drawSingleLineOfText(
        message, display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message) / 2),
        display.yAbsolute(display.yAbsoluteCenter() + yAdjust + 4), invert);
}

void DemoScreen::finishDemo() {
    // When we're done with the onboarding screens, turn this mode off.
    dropVcom_ = false;
    display_.invertDisplay(false);
    prefs_.noSetupMode.set(true);

    lightingController_.disableOnlyIncreaseMode();

    back();
}

void DemoScreen::render(DisplaySystem &display) {
    if ((currentScreen_ > lambdas_.size() - 1) || (bailAfterLens_ && currentScreen_ > 1)) {
        if (bailAfterLens_) {
            display_.clearDontChangeRefreshType();
            display_.clearDisplay();
            display_.clearFrameBuffer();
            lightingController_.setScreenMask(LightingController::Screens::BOTH);
        }
        finishDemo();
        return;
    }

    // Alternate left and right on for setup. Note: This seems backwards
    // but it looks right in-glass. Are LEFT and RIGHT backwards in the LightingController?
    if (currentScreen_ == 1) {
        lightingController_.setScreenMask(LightingController::Screens::LEFT);
    } else if (currentScreen_ == 0) {
        lightingController_.setScreenMask(LightingController::Screens::RIGHT);
    }

    // Turn on both screen lights for last case
    if (currentScreen_ == 2) {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
    }

    // @john -- tada :)
    if (currentScreen_ == lambdas_.size() - 1) {
#if !SIMULATOR
        if (!WiFi::getInstance().hasWiFiCredentials()) {
#else
        if (1) {
#endif
            lambdas_[currentScreen_](display);
        } else {
            finishDemo();
            return;
        }
    } else {
        lambdas_[currentScreen_](display);
    }

    if (dropVcom_) {
        nextVcomNudge_ = -2500;
    } else {
        nextVcomNudge_ = 0;
    }

    if ((currentScreen_ >= 3) && (currentScreen_ != lambdas_.size() - 1)) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        display.updateDisplay(nextUpdateType_, true, nextVcomNudge_);
        currentScreen_++;
    } else if (currentScreen_ < 3) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        currentSubScreen_++;
    } else {
        // App setup, last screen. Don't advance. Just wait for the button press.
        display.updateDisplay(nextUpdateType_, true, nextVcomNudge_);
    }
}

void DemoScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
        if ((currentScreen_ > 0) && (currentScreen_ < 3)) {
            display_.clearFrameBuffer();
            shownOne_ = false;
            shownTwo_ = false;
            currentScreen_--;
        }
        currentSubScreen_ = 0;
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
        if (currentScreen_ >= 9) {
            xTimerStop(DisplaySystem::animationTimer, 0);
#if !SIMULATOR
            if (!WiFi::getInstance().hasWiFiCredentials() && !shownAppDownload_) {
#else
            if (!shownAppDownload_) {
#endif

                xTimerStop(DisplaySystem::animationTimer, 0);
                // Restart the timer if they stopped it.
                xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(5), 0);
                currentScreen_ = lambdas_.size() - 1;
                shownAppDownload_ = true;
            } else {
                // Gives us the full refresh
                display_.clearDontChangeRefreshType();
                display_.clearDisplay();
                display_.clearFrameBuffer();
                finishDemo();
            }
        } else if (currentScreen_ < 3) {
            currentScreen_++;
            currentSubScreen_ = 0;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::UP:
        // If we're on page 0 or 1 and the user hits up, show the serial number.
        if (currentScreen_ <= 1) {
            drawSerial(display_);
        }
        break;
    case RemoteCommand::RIGHT:
    case RemoteCommand::DOWN:
    case RemoteCommand::BACK:
        break;
    case RemoteCommand::ANIMATION:
        setNeedsRerender();
        break;
    default:
        break;
    }
}
