#include "UI/CesScreen.hpp"

#include "sindarin-debug.h"

extern void UIInnerLoop(RemoteCommand command);

CesScreen::CesScreen(DisplaySystem &display, PreferencesStore &prefs,
                     LightingController &lightingController)
    : display_(display), renderer_(display), prefs_(prefs),
      lightingController_(lightingController) {}

void CesScreen::render(DisplaySystem &display) {
    std::string message = messages_[currentScreen_];

    display.setFont(static_cast<Font *>(&fontTahomaRegularFonT6PT7B));

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

    // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
    switch (currentScreen_) {
    case 0:
    case 1:
        display.setFont(static_cast<Font *>(&fontTahomaRegularFonT8PT7B));
        break;
    case 2:
        display.setFont(static_cast<Font *>(&fontTahomaRegularFonT5PT7B));
        break;
    }
    // NOLINTEND(bugprone-branch-clone)

    // This Y coordinate is just a guess
    display.drawSingleLineOfText(
        message.c_str(),
        display.xAbsolute(display.xAbsoluteCenter() - display.getTextWidth(message.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - display.getTextHeight() / 2));
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void CesScreen::processCommand(RemoteCommand command) {
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
        setNeedsRerender();
        break;
    default:
        break;
    }

    if (currentScreen_ >= NUM_CES_SCREENS) {
        back();
    }
}
