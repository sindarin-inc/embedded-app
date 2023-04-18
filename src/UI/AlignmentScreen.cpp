#include "UI/AlignmentScreen.hpp"

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

AlignmentScreen::AlignmentScreen(DisplaySystem &display, PreferencesStore &prefs,
                                 LightingController &lightingController)
    : display_(display), prefs_(prefs), lightingController_(lightingController) {}

void AlignmentScreen::render(DisplaySystem &display) {
    // Alternate left and right on for setup.
    if (currentScreen_ == 1) {
        lightingController_.setScreenMask(LightingController::Screens::LEFT);
    } else if (currentScreen_ == 0) {
        lightingController_.setScreenMask(LightingController::Screens::RIGHT);
    }
    // Turn on both screen lights for last case
    else /*( currentScreen_ >= 2) */
    {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
    }
    // Don't need to set it after that as they are on.

    display.fillCircle(display.xAbsolute(display.xAbsoluteCenter()),
                       display.yAbsolute(display.yAbsoluteCenter()), 5, display.fg);
    display.updateDisplay(DisplaySystem::UpdateType::FULL);
}

void AlignmentScreen::processCommand(RemoteCommand command) {
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
        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
    default:
        break;
    }

    if (currentScreen_ >= 2) {
        lightingController_.setScreenMask(LightingController::Screens::BOTH);
        backCallback();
    }
}
