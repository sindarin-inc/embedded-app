#include "UI/AlignmentScreen.hpp"

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

AlignmentScreen::AlignmentScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), prefs_(prefs) {}

void AlignmentScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

#if !SIMULATOR
    auto backlight = prefs_.backlight.get().value();

    // Alternate left and right on for setup.
    if (currentScreen_ == 1) {
        SetRightBackLight(0);
        SetLeftBackLight(backlight);
    } else {
        SetRightBackLight(backlight);
        SetLeftBackLight(0);
    }

    // Turn on both backlights for last case
    if (currentScreen_ >= 2) {
        SetRightBackLight(backlight);
        SetLeftBackLight(backlight);
    }
    // Don't need to set it after that as they are on.
#endif

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
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
    default:
        break;
    }

    if (currentScreen_ >= 2) {
#if !SIMULATOR
        auto backlight = prefs_.backlight.get().value();
        SetRightBackLight(backlight);
        SetLeftBackLight(backlight);
#endif
        backCallback();
    }
}
