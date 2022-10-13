#include "UI/DemoScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/Bitmaps/Logo.hpp"
#include "UI/Bitmaps/stone.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "sindarin-debug.h"

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {}

//
// for the size parameter, each 1 is 10 pixels in o radius (e.g., 1 is 10 raidus, 2 is 20 radius,
// etc).
//

void DemoScreen::drawSolLogo(DisplaySystem &display, int x, int y, int size, bool solid) {

    int diameter = size * 10;
    int letterDelta = size;

    if (size == 1) {
        letterDelta++;
    } else {
        letterDelta--;
    }

    int radius = diameter / 2;

    // draw o (the S is 90% the width, hence subtracting out size)
    int oX = x + diameter + letterDelta - size;
    int oY = y;
    // the Circle draw commands take the center point so we add the radius to the above
    // calculated values since everything else wants upper left.
    if (solid) {
        display.fillCircle(oX + radius, oY + radius, radius, display.fg);
    } else {
        display.drawCircle(oX + radius, oY + radius, radius, display.fg);
    }

    // draw l
    int lX = oX + diameter + letterDelta + 1;
    int lY = y - diameter * 2 / 10;
    if (solid) {
        display.fillRect(lX, lY, diameter * 6 / 10, diameter * 12 / 10 + 1, display.fg);
    } else {
        display.drawRect(lX, lY, diameter * 6 / 10, diameter * 12 / 10 + 1, display.fg);
    }

    // draw s
    display.fillRoundRect(x, y, 9 * diameter / 10, diameter, diameter / 2, display.fg);
    display.fillRect(x + 4 * diameter / 10, y, 5 * diameter / 10, diameter / 2, display.fg);
    display.fillRect(x, y + 6 * diameter / 10, diameter / 2, 4 * diameter / 10, display.fg);

    if (!solid) {
        display.fillRoundRect(x + 1, y + 1, 9 * diameter / 10 - 2, diameter - 2, diameter / 2,
                              display.bg);
        display.fillRect(x + 4 * diameter / 10 + 1, y + 1, 5 * diameter / 10 - 2, diameter / 2 - 2,
                         display.bg);
        display.fillRect(x + 1, y + 6 * diameter / 10 + 1, diameter / 2 - 2, 4 * diameter / 10 - 2,
                         display.bg);
    }
}

void DemoScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    bool dontRefresh = false;

    std::string message = messages_[currentScreen_];

    display.setFont(&TAHOMA_REGULAR_FONT6PT7B);

    // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
    switch (currentScreen_) {
    case 0:
        display.setFont(&TAHOMA_REGULAR_FONT10PT7B);
        break;
    case 1:
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
        break;
    case 2:
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
        break;
    case 3:
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
        break;
    }
    // NOLINTEND(bugprone-branch-clone)
    if (currentScreen_ > 1) {
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);

        if (currentScreen_ > 3) {
            display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        }

        if (currentScreen_ > 3) {
            // drawSolLogo(display, 50, 120, 3 /*display.height() - 4*/, true);
            /*
                        display.fillRoundRect(2, 2, display.width() - 4, display.height() - 4, 50,
               display.fg); display.fillRect(display.width() - 54, 2, 52, 52, display.fg);
                        display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                        display.fillRoundRect(3, 3, display.width() - 6, display.height() - 6, 50,
               display.bg); display.fillRect(display.width() - 54, 3, 51, 51, display.bg);
                        display.fillRect(3, display.height() - 54, 51, 51, display.bg);
             */
        }

        if (currentScreen_ > 3) {
            switch (currentScreen_) {

            case 4:
                message = "Put the PageTurner down and read.";
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str())));
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(1000);
                display.clearDisplay();

                message = DEMO_SCREEN_5;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str())));
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(1500);
                /* break;

             case 5:*/
                message = DEMO_SCREEN_6;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        7 * display.getTextHeight(message.c_str()));
                message = DEMO_SCREEN_6a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        5.5 * display.getTextHeight(message.c_str()));

                display.drawCircle(display.width() / 2, 100, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 100, display.width() / 2 + 12, 100,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108,
                                 display.fg);

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(250);

                message = DEMO_SCREEN_6;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        7 * display.getTextHeight(message.c_str()));
                message = DEMO_SCREEN_6a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        5.5 * display.getTextHeight(message.c_str()));

                display.drawCircle(display.width() / 2, 100, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 100, display.width() / 2 + 12, 100,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 115, display.width() / 2 + 12, 115,
                                 display.fg);

                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                delay(750);

                message = DEMO_SCREEN_6;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        7 * display.getTextHeight(message.c_str()));
                message = DEMO_SCREEN_6a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        5.5 * display.getTextHeight(message.c_str()));

                display.drawCircle(display.width() / 2, 100, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 100, display.width() / 2 + 12, 100,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 115, display.width() / 2 + 12, 115,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 122, display.width() / 2 + 12, 122,
                                 display.fg);

                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                delay(750);

                message = DEMO_SCREEN_6;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        7 * display.getTextHeight(message.c_str()));
                message = DEMO_SCREEN_6a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        5.5 * display.getTextHeight(message.c_str()));

                display.drawCircle(display.width() / 2, 100, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 100, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 100, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 100, display.width() / 2 + 12, 100,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 108, display.width() / 2 + 12, 108,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 115, display.width() / 2 + 12, 115,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 122, display.width() / 2 + 12, 122,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 129, display.width() / 2 + 8, 129,
                                 display.fg);
                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                dontRefresh = true;
                message = "";
                display.clearDisplay();
                delay(750);
                /*break;
            case 6:*/

                message = DEMO_SCREEN_7;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2));
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(1500);
                /*
                                break;
                            case 7:*/

                message = DEMO_SCREEN_8;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        4.5 * display.getTextHeight(message.c_str()));

                message = DEMO_SCREEN_8a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        3 * display.getTextHeight(message.c_str()));
                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - papyrus2_width / 2), 90,
                    papyrus2_bits, papyrus2_width, papyrus2_height, display.fg);
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::FULL);
                delay(3000);
                /*display.clearDisplay();

                break;
            case 8:*/

                message = DEMO_SCREEN_9;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2));
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(1500);
                /*
                                break;
                            case 9:*/

                message = DEMO_SCREEN_10;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2) -
                        6);
                display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - paper_width / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - paper_height / 2),
                                    paper_bits, paper_width, paper_height, display.fg);
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(1500);
                /*
                                break;
                            case 10:*/

                message = DEMO_SCREEN_11;

                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2 + 35),
                    display.yAbsolute(58 + hi_hand_height / 2 -
                                      display.getTextHeight(message.c_str()) / 2));

                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45, 30,
                    hi_hand_bits, hi_hand_width, hi_hand_height, display.fg);
                display.updateDisplay(DisplaySystem::UpdateType::FULL);
                delay(1500);
                message = DEMO_SCREEN_11;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2 + 35),
                    display.yAbsolute(58 + hi_hand_height / 2 -
                                      display.getTextHeight(message.c_str()) / 2));
                message = DEMO_SCREEN_11a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2 + 35),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2));

                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45, 30,
                    hi_hand_bits, hi_hand_width, hi_hand_height, display.fg);

                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_blocks_width / 2) - 45,
                    display.yAbsolute(display.yAbsoluteCenter() - hi_blocks_height / 2),
                    hi_blocks_bits, hi_blocks_width, hi_blocks_height, display.fg);
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(1500);
                /* message = DEMO_SCREEN_11;
                 display.drawSingleLineOfText(
                     message.c_str(),
                     display.xAbsolute(display.xAbsoluteCenter() -
                                       display.getTextWidth(message.c_str()) / 2 + 35),
                     display.yAbsolute(58 + hi_hand_height / 2 -
                                       display.getTextHeight(message.c_str()) / 2));
                 message = DEMO_SCREEN_11a;
                 display.drawSingleLineOfText(
                     message.c_str(),
                     display.xAbsolute(display.xAbsoluteCenter() -
                                       display.getTextWidth(message.c_str()) / 2 + 35),
                     display.yAbsolute(display.yAbsoluteCenter() -
                                       display.getTextHeight(message.c_str()) / 2));
                 message = DEMO_SCREEN_11b;
                 display.drawSingleLineOfText(
                     message.c_str(),
                     display.xAbsolute(display.xAbsoluteCenter() -
                                       display.getTextWidth(message.c_str()) / 2 + 35),
                     display.yAbsolute(display.yAbsoluteCenter() + 45));

                 display.drawXBitmap(
                     display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45, 30,
                     hi_hand_bits, hi_hand_width, hi_hand_height, display.fg);

                 display.drawXBitmap(
                     display.xAbsolute(display.xAbsoluteCenter() - hi_blocks_width / 2) - 45,
                     display.yAbsolute(display.yAbsoluteCenter() - hi_blocks_height / 2),
                     hi_blocks_bits, hi_blocks_width, hi_blocks_height, display.fg);

                 display.drawXBitmap(
                     display.xAbsolute(display.xAbsoluteCenter() - hi_pixel3_width / 2) - 45, 130,
                     hi_pixel1_bits, hi_pixel1_width, hi_pixel1_height, display.fg);
                 display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                 delay(500);*/
                message = DEMO_SCREEN_11;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2 + 35),
                    display.yAbsolute(58 + hi_hand_height / 2 -
                                      display.getTextHeight(message.c_str()) / 2));
                message = DEMO_SCREEN_11a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2 + 35),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2));
                message = DEMO_SCREEN_11b;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2 + 35),
                    display.yAbsolute(display.yAbsoluteCenter() + 45));
                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_hand_width / 2) - 45, 30,
                    hi_hand_bits, hi_hand_width, hi_hand_height, display.fg);

                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_blocks_width / 2) - 45,
                    display.yAbsolute(display.yAbsoluteCenter() - hi_blocks_height / 2),
                    hi_blocks_bits, hi_blocks_width, hi_blocks_height, display.fg);

                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_pixel3_width / 2) - 45, 130,
                    hi_pixel1_bits, hi_pixel1_width, hi_pixel1_height, display.fg);
                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - hi_pixel3_width / 2) - 45, 130,
                    hi_pixel3_bits, hi_pixel3_width, hi_pixel3_height, display.fg);
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                dontRefresh = true;
                display.clearDisplay();
                /*
                break;
            case 11:
*/
                delay(1500);
                message = DEMO_SCREEN_12;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2));
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                message = "";

                delay(1500);
                /*
                                break;
                            case 12:*/
                message = DEMO_SCREEN_13;
                display.drawSingleLineOfText(
                    message.c_str(), display.xAbsolute(display.xAbsoluteCenter() - 65),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      1.5 * display.getTextHeight(message.c_str())));

                message = DEMO_SCREEN_13a;
                display.drawSingleLineOfText(
                    message.c_str(), display.xAbsolute(display.xAbsoluteCenter() - 65),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      0 * display.getTextHeight(message.c_str())));
                message = DEMO_SCREEN_13b;
                display.drawSingleLineOfText(
                    message.c_str(), display.xAbsolute(display.xAbsoluteCenter() - 65),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 +
                                      1.5 * display.getTextHeight(message.c_str())));
                display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - spark_width / 2) +
                                        55,
                                    45, spark_bits, spark_width, spark_height, display.fg);
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

                delay(4000);
                /*
                break;
            case 13:
*/
                message = DEMO_SCREEN_14;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      3 * display.getTextHeight(message.c_str())));

                message = DEMO_SCREEN_14a;
                display.drawSingleLineOfText(
                    message.c_str(),
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

                message = "";

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

                delay(4000);
                /*
                break;
            case 14:
                */

                message = DEMO_SCREEN_15;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2));
                message = "";
                display.clearDisplay();

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

                delay(2000);
                /*
                                break;
                            case 15:
                */
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

                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3,
                                   display.fg);

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(500);

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

                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3,
                                   display.fg);

                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 2, 125 - 2, 4,
                                   display.fg);
                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                delay(500);

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

                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3,
                                   display.fg);

                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 2, 125 - 2, 4,
                                   display.fg);
                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 + 10, 125 - 11, 5,
                                   display.fg);

                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                delay(500);

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

                display.drawCircle(display.xAbsoluteCenter() - thought_width / 2 - 7, 129, 3,
                                   display.fg);

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
                dontRefresh = true;
                display.clearDisplay();
                delay(1500);
                message = "";
                /*
                                break;
                            case 16:
                */
                message = DEMO_SCREEN_17;
                display.drawSingleLineOfText(
                    message.c_str(),
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
                display.drawSingleLineOfText(
                    message.c_str(),
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

                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2) +
                        2,
                    display.yAbsolute(display.yAbsoluteCenter() + 10));
                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - lightbulb_width / 2), 90,
                    lightbulb_bits, lightbulb_width, lightbulb_height, display.fg);

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                dontRefresh = true;
                delay(1500);
                message = "";
                /*
                break;
            case 17:
*/
                message = DEMO_SCREEN_18;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      8 * display.getTextHeight(message.c_str())));
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(750);

                message = DEMO_SCREEN_18;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      8 * display.getTextHeight(message.c_str())));
                message = DEMO_SCREEN_18a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2) +
                        30,
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      2 * display.getTextHeight(message.c_str())));

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(500);

                message = DEMO_SCREEN_18;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      8 * display.getTextHeight(message.c_str())));
                message = DEMO_SCREEN_18a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2) +
                        30,
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      2 * display.getTextHeight(message.c_str())));

                message = DEMO_SCREEN_18b;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2) -
                        30,
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 +
                                      1 * display.getTextHeight(message.c_str())));

                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(500);

                message = DEMO_SCREEN_18;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      8 * display.getTextHeight(message.c_str())));
                message = DEMO_SCREEN_18a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2) +
                        30,
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 -
                                      2 * display.getTextHeight(message.c_str())));

                message = DEMO_SCREEN_18b;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2) -
                        30,
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 +
                                      1 * display.getTextHeight(message.c_str())));

                message = DEMO_SCREEN_18c;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() -
                                      display.getTextHeight(message.c_str()) / 2 +
                                      4 * display.getTextHeight(message.c_str())));
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                dontRefresh = true;

                message = "";
                delay(1500);
                display.clearDisplay();
                /*
                break;
            case 18:
*/
                message = DEMO_SCREEN_19;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 -
                                      5 * display.getTextHeight()));
                message = DEMO_SCREEN_19a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 -
                                      3.5 * display.getTextHeight()));
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(2000);
                message = DEMO_SCREEN_19;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 -
                                      5 * display.getTextHeight()));
                message = DEMO_SCREEN_19a;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 -
                                      3.5 * display.getTextHeight()));
                display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - franklin_width / 2), 80,
                    franklin_bits, franklin_width, franklin_height, display.fg);
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                dontRefresh = true;
                message = "";
                delay(1500);
                /*
                break;
            case 19:
*/
                message = DEMO_SCREEN_20;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 -
                                      1.5 * display.getTextHeight()));

                message = DEMO_SCREEN_20a;

                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 -
                                      0 * display.getTextHeight()));
                message = DEMO_SCREEN_20b;
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2 +
                                      1.5 * display.getTextHeight()));
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(5000);
                /*
                                break;
                            case 20:*/
                message = "";

                drawSolLogo(display, 47, 87, 3, true);
                break;

                /*
                 */

                /**/
                /*
                 */
                /**/

                /**/

                /*display.drawXBitmap(
                    display.xAbsolute(display.xAbsoluteCenter() - thought_width / 2), 10,
                    thought_bits, thought_width, thought_height, display.fg);*/
                /*

                */
                /* display.drawXBitmap(display.xAbsolute(display.xAbsoluteCenter() - phone_width /
                   2), 10, phone_bits, phone_width, phone_height, display.fg);*/
                /*


                                message = "";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(65));
                                message = "";
                                break;
                            case 6:

                                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 +
                   12, 120, display.fg);

                                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2
                   + 12, 128, display.fg);

                                // display.drawLine(display.width() / 2 - 12, 135, display.width() /
                   2 + 12, 135,
                                //                  display.fg);

                                // display.drawLine(display.width() / 2 - 12, 142, display.width() /
                   2 + 12, 142,
                                //                  display.fg);

                                // display.drawLine(display.width() / 2 - 12, 149, display.width() /
                   2 + 8, 149,
                                //                  display.fg);
                                //  display.fillRect(26, 26, display.width() - 51, display.height()
                   - 51,
                                //  display.bg);

                                message = "Five thousand years ago,";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 2 *
                   display.getTextHeight() - 40));

                                message = "these words would have";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() -
                   display.getTextHeight() - 40));

                                message = "been etched in stone.";

                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                                message = "";
                                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                                delay(200);

                                display.fillRoundRect(2, 2, display.width() - 4, display.height() -
                   4, 50, display.fg); display.fillRect(display.width() - 54, 2, 52, 52,
                   display.fg); display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                                display.fillRoundRect(4, 4, display.width() - 8, display.height() -
                   8, 50, display.bg); display.fillRect(display.width() - 56, 4, 52, 52,
                   display.bg); display.fillRect(4, display.height() - 56, 52, 52, display.bg);

                                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 +
                   12, 120, display.fg);

                                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2
                   + 12, 128, display.fg);

                                display.drawLine(display.width() / 2 - 12, 135, display.width() / 2
                   + 12, 135, display.fg);

                                // display.drawLine(display.width() / 2 - 12, 142, display.width() /
                   2 + 12, 142,
                                //                  display.fg);

                                // display.drawLine(display.width() / 2 - 12, 149, display.width() /
                   2 + 8, 149,
                                //                 display.fg);
                                // display.fillRect(26, 26, display.width() - 51, display.height() -
                   51,
                                // display.bg);
                                message = "Five thousand years ago,";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 2 *
                   display.getTextHeight() - 40));

                                message = "these words would have";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() -
                   display.getTextHeight() - 40));

                                message = "been etched in stone.";

                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                                message = "";

                                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                                delay(750);

                                display.fillRoundRect(2, 2, display.width() - 4, display.height() -
                   4, 50, display.fg); display.fillRect(display.width() - 54, 2, 52, 52,
                   display.fg); display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                                display.fillRoundRect(4, 4, display.width() - 8, display.height() -
                   8, 50, display.bg); display.fillRect(display.width() - 56, 4, 52, 52,
                   display.bg); display.fillRect(4, display.height() - 56, 52, 52, display.bg);
                                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 +
                   12, 120, display.fg);

                                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2
                   + 12, 128, display.fg);

                                display.drawLine(display.width() / 2 - 12, 135, display.width() / 2
                   + 12, 135, display.fg);

                                display.drawLine(display.width() / 2 - 12, 142, display.width() / 2
                   + 12, 142, display.fg);

                                // display.drawLine(display.width() / 2 - 12, 149, display.width() /
                   2 + 8, 149,
                                //                  display.fg);
                                //  display.fillRect(26, 26, display.width() - 51, display.height()
                   - 51,
                                //  display.bg);

                                message = "Five thousand years ago,";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 2 *
                   display.getTextHeight() - 40));

                                message = "these words would have";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() -
                   display.getTextHeight() - 40));

                                message = "been etched in stone.";

                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                                message = "";

                                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                                delay(750);

                                display.fillRoundRect(2, 2, display.width() - 4, display.height() -
                   4, 50, display.fg); display.fillRect(display.width() - 54, 2, 52, 52,
                   display.fg); display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                                display.fillRoundRect(4, 4, display.width() - 8, display.height() -
                   8, 50, display.bg); display.fillRect(display.width() - 56, 4, 52, 52,
                   display.bg); display.fillRect(4, display.height() - 56, 52, 52, display.bg);
                                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 +
                   12, 120, display.fg);

                                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2
                   + 12, 128, display.fg);

                                display.drawLine(display.width() / 2 - 12, 135, display.width() / 2
                   + 12, 135, display.fg);

                                display.drawLine(display.width() / 2 - 12, 142, display.width() / 2
                   + 12, 142, display.fg);

                                display.drawLine(display.width() / 2 - 12, 149, display.width() / 2
                   + 8, 149, display.fg);
                                //  display.fillRect(26, 26, display.width() - 51, display.height()
                   - 51,
                                //  display.bg);

                                message = "Five thousand years ago,";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 2 *
                   display.getTextHeight() - 40));

                                message = "these words would have";
                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() -
                   display.getTextHeight() - 40));

                                message = "been etched in stone.";

                                display.drawSingleLineOfText(
                                    message.c_str(),
                                    display.xAbsolute(display.xAbsoluteCenter() -
                                                      display.getTextWidth(message.c_str()) / 2),
                                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                                message = "";

                                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                                dontRefresh = true;
                                break;

                            case 7:

                                break;

                            case 8:

                                break;

                            case 9:

                                break;*/
            }
        }

        // This Y coordinate is just a guess
        display.drawSingleLineOfText(
            message.c_str(),
            display.xAbsolute(display.xAbsoluteCenter() -
                              display.getTextWidth(message.c_str()) / 2),
            display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2));

        if (!dontRefresh) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        }

    }
    // Draw some dots
    else {
        display.fillCircle(display.xAbsolute(display.xAbsoluteCenter()),
                           display.yAbsolute(display.yAbsoluteCenter()), 5, display.fg);

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

#if !SIMULATOR
    auto backlight = prefs_.backlight.get().value();

    // Alternate left and right on for setup.
    if (currentScreen_ % 2 == 1) {
        SetRightBackLight(0);
        SetLeftBackLight(backlight);
    } else {
        SetRightBackLight(backlight);
        SetLeftBackLight(0);
    }

    // Turn on both backlights for last case
    if (currentScreen_ >= 4) {
        SetRightBackLight(backlight);
        SetLeftBackLight(backlight);
    }
#endif
    needsRerender = false;
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
        currentScreen_++;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
    default:
        break;
    }

    if (currentScreen_ >= NUM_DEMO_SCREENS) {
#if !SIMULATOR
        auto backlight = prefs_.backlight.get().value();
        SetRightBackLight(backlight);
        SetLeftBackLight(backlight);
#endif
        // When we're done with the onboarding screen, turn this mode off.
        prefs_.demoMode.set(0);
        backCallback();
    }
}
