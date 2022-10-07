#include "UI/DemoScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "sindarin-debug.h"

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {}

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
    case 4:
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
        break;
    }
    // NOLINTEND(bugprone-branch-clone)
    if (currentScreen_ > 1) {
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);

        if (currentScreen_ > 4) {
            display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        }

        if (currentScreen_ > 3) {
            display.fillRoundRect(2, 2, display.width() - 4, display.height() - 4, 50, display.fg);
            display.fillRect(display.width() - 54, 2, 52, 52, display.fg);
            display.fillRect(2, display.height() - 54, 52, 52, display.fg);

            display.fillRoundRect(4, 4, display.width() - 8, display.height() - 8, 50, display.bg);
            display.fillRect(display.width() - 56, 4, 52, 52, display.bg);
            display.fillRect(4, display.height() - 56, 52, 52, display.bg);
        }

        if (currentScreen_ > 4) {
            switch (currentScreen_) {
            case 5:
                message = "Five thousand years ago";
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2));
                break;
            case 6:

                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 + 12, 120,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2 + 12, 128,
                                 display.fg);

                // display.drawLine(display.width() / 2 - 12, 135, display.width() / 2 + 12, 135,
                //                  display.fg);

                // display.drawLine(display.width() / 2 - 12, 142, display.width() / 2 + 12, 142,
                //                  display.fg);

                // display.drawLine(display.width() / 2 - 12, 149, display.width() / 2 + 8, 149,
                //                  display.fg);
                //  display.fillRect(26, 26, display.width() - 51, display.height() - 51,
                //  display.bg);

                message = "these words would have";
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() - 40));

                message = "been etched in stone.";

                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                message = "";
                display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
                delay(200);

                display.fillRoundRect(2, 2, display.width() - 4, display.height() - 4, 50,
                                      display.fg);
                display.fillRect(display.width() - 54, 2, 52, 52, display.fg);
                display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                display.fillRoundRect(4, 4, display.width() - 8, display.height() - 8, 50,
                                      display.bg);
                display.fillRect(display.width() - 56, 4, 52, 52, display.bg);
                display.fillRect(4, display.height() - 56, 52, 52, display.bg);

                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 + 12, 120,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2 + 12, 128,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 135, display.width() / 2 + 12, 135,
                                 display.fg);

                // display.drawLine(display.width() / 2 - 12, 142, display.width() / 2 + 12, 142,
                //                  display.fg);

                // display.drawLine(display.width() / 2 - 12, 149, display.width() / 2 + 8, 149,
                //                 display.fg);
                // display.fillRect(26, 26, display.width() - 51, display.height() - 51,
                // display.bg);

                message = "these words would have";
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() - 40));

                message = "been etched in stone.";

                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                message = "";

                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                delay(750);

                display.fillRoundRect(2, 2, display.width() - 4, display.height() - 4, 50,
                                      display.fg);
                display.fillRect(display.width() - 54, 2, 52, 52, display.fg);
                display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                display.fillRoundRect(4, 4, display.width() - 8, display.height() - 8, 50,
                                      display.bg);
                display.fillRect(display.width() - 56, 4, 52, 52, display.bg);
                display.fillRect(4, display.height() - 56, 52, 52, display.bg);
                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 + 12, 120,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2 + 12, 128,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 135, display.width() / 2 + 12, 135,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 142, display.width() / 2 + 12, 142,
                                 display.fg);

                // display.drawLine(display.width() / 2 - 12, 149, display.width() / 2 + 8, 149,
                //                  display.fg);
                //  display.fillRect(26, 26, display.width() - 51, display.height() - 51,
                //  display.bg);

                message = "these words would have";
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() - 40));

                message = "been etched in stone.";

                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - 40));
                message = "";

                display.updateDisplay(DisplaySystem::UpdateType::FAST);
                delay(750);

                display.fillRoundRect(2, 2, display.width() - 4, display.height() - 4, 50,
                                      display.fg);
                display.fillRect(display.width() - 54, 2, 52, 52, display.fg);
                display.fillRect(2, display.height() - 54, 52, 52, display.fg);

                display.fillRoundRect(4, 4, display.width() - 8, display.height() - 8, 50,
                                      display.bg);
                display.fillRect(display.width() - 56, 4, 52, 52, display.bg);
                display.fillRect(4, display.height() - 56, 52, 52, display.bg);
                display.drawCircle(display.width() / 2, 120, 25, display.fg);
                display.drawRect(display.width() / 2 - 25, 120, 51, 50, display.fg);
                display.fillRect(display.width() / 2 - 24, 120, 49, 49, display.bg);

                display.drawLine(display.width() / 2 - 8, 120, display.width() / 2 + 12, 120,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 128, display.width() / 2 + 12, 128,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 135, display.width() / 2 + 12, 135,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 142, display.width() / 2 + 12, 142,
                                 display.fg);

                display.drawLine(display.width() / 2 - 12, 149, display.width() / 2 + 8, 149,
                                 display.fg);
                //  display.fillRect(26, 26, display.width() - 51, display.height() - 51,
                //  display.bg);

                message = "these words would have";
                display.drawSingleLineOfText(
                    message.c_str(),
                    display.xAbsolute(display.xAbsoluteCenter() -
                                      display.getTextWidth(message.c_str()) / 2),
                    display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() - 40));

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

                break;
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
