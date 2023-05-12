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
#include "UI/Fonts/GFXFonts/Tahoma10pt7b.h"
#include "UI/Fonts/GFXFonts/Tahoma20pt7b.h"
#include "UI/Fonts/GFXFonts/Tahoma5pt7b.h"
#include "UI/Fonts/GFXFonts/Tahoma6pt7b.h"
#include "UI/Fonts/GFXFonts/Tahoma7pt7b.h"
#include "UI/Fonts/GFXFonts/Tahoma8pt7b.h"
#include "UI/Fonts/GFXFonts/Tahoma9pt7b.h"
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
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = "like a binocular. When text is";
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = "crisp, press \ue000 on your remote.";
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
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
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false,
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
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = "like a binocular. When text is";
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = "crisp, press \ue000 on your remote.";
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
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
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false,
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
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset));
            message = "to adjust lens spacing. When ";
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
                display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                  textYOffset + 17));
            message = "text is comfortable, press \ue000";
            display.drawSingleLineOfText(
                message.c_str(),
                display.xAbsolute(display.xAbsoluteCenter() -
                                  display.getTextWidth(message.c_str()) / 2),
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
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false,
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
        std::string message = "Now put down the Sol Remote and enjoy!";
        display.clearDisplay();
        display.clearFrameBuffer();
        display.setFont(static_cast<Font *>(&ibmf75Face0));
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str())));
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 3000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), BK1_BITS, bk1_width,
                            bk1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), BK2_BITS, bk2_width,
                            bk2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), BK3_BITS, bk3_width,
                            bk3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), BK4_BITS, bk4_width,
                            bk4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        drawSkipMessage(display, true);
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.clearDisplay();
        drawDoubleLine(display, "The way we read", "is always changing.");
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
        drawDoubleLine(display, "What reading can do", "for our minds:");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawDoubleLine(display, "Whisking us away", "into other realms.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SP1_BITS, sp1_width,
                            sp1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SP2_BITS, sp2_width,
                            sp2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SP3_BITS, sp3_width,
                            sp3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SP4_BITS, sp4_width,
                            sp4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        drawDoubleLine(display, "Sparking new", "thoughts and ideas.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), IDEA1_BITS, idea1_width,
                            idea1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), IDEA2_BITS, idea2_width,
                            idea2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), IDEA3_BITS, idea3_width,
                            idea3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), IDEA4_BITS, idea4_width,
                            idea4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(true);
        drawTripleLine(display, "In this era of", "constant beeps,", "alerts, and updates...");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(false);
        display.clearDisplay();
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), PHONE1_BITS, phone1_width,
                            phone1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), PHONE2_BITS, phone2_width,
                            phone2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), PHONE3_BITS, phone3_width,
                            phone3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(true);
        drawTripleLine(display, "...your Sol Reader", "provides a quiet", "respite from it all.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(false);
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SOL_READER1_BITS,
                            sol_reader1_width, sol_reader1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(true);
        drawTripleLine(display, "Simply lean back,", "open your eyes, and",
                       "let words light the way.");
        drawSkipMessage(display);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(false);
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), EYE1_BITS, eye1_width,
                            eye1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), EYE2_BITS, eye2_width,
                            eye2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), EYE3_BITS, eye3_width,
                            eye3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM10_BITS, im10_width, im10_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM20_BITS, im20_width, im20_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM30_BITS, im30_width, im30_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM40_BITS, im40_width, im40_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM50_BITS, im50_width, im50_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM60_BITS, im60_width, im60_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM70_BITS, im70_width, im70_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM80_BITS, im80_width, im80_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im10_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im10_height / 2),
                            IM90_BITS, im90_width, im90_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 10;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SPACE_SCAPE1_BITS,
                            space_scape1_width, space_scape1_height, display.fg);
        display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - im100_width / 2),
                            display.yAbsolute(display.yAbsoluteCenter() - im100_height / 2),
                            IM100_BITS, im100_width, im100_height, display.bg);
        nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
        nextInterval_ = 1500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SOL_SCROLL_SMALL_BITS,
                            sol_scroll_small_width, sol_scroll_small_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap(display.xAbsolute(0), display.yAbsolute(0), SOL_SCROLL_LARGE_BITS,
                            sol_scroll_large_width, sol_scroll_large_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 750;
    });
}

void DemoScreen::drawSkipMessage(DisplaySystem &display, bool inverted) {
    std::string message = "Press \ue000 to skip";
    display.setFont(static_cast<Font *>(&ibmf75Face0));
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(235), inverted);
}

void DemoScreen::drawDoubleLine(DisplaySystem &display, std::string one, std::string two) {
    std::string message;
    display.setFont(static_cast<Font *>(&ibmf75Face1));
    message = std::move(one);
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str()) / 2) -
            4);
    message = std::move(two);
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() + display.getTextHeight(message.c_str()) / 2));
}

void DemoScreen::drawTripleLine(DisplaySystem &display, std::string one, std::string two,
                                std::string three) {
    std::string message;
    display.setFont(static_cast<Font *>(&ibmf75Face1));
    message = std::move(one);
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight(message.c_str()) - 8));
    message = std::move(two);
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - 3));
    message = std::move(three);
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() + display.getTextHeight(message.c_str())));
}

void DemoScreen::finishDemo() {
    // When we're done with the onboarding screens, turn this mode off.
    prefs_.demoMode.remove();
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
    lambdas_[currentScreen_](display);

    if (currentScreen_ >= 3) {
        display.updateDisplay(nextUpdateType_);
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        currentScreen_++;
    } else {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        currentSubScreen_++;
    }
}

void DemoScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
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
        xTimerStop(DisplaySystem::animationTimer, 0);
        if (currentScreen_ >= 3) {
            // Gives us the full refresh
            display_.clearDontChangeRefreshType();
            display_.clearDisplay();
            display_.clearFrameBuffer();
            finishDemo();
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
        break;
    case RemoteCommand::ANIMATION:
        setNeedsRerender();
        break;
    default:
        break;
    }
}
