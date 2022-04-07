#include "UI/DemoScreen.hpp"

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

    std::string message = messages_[currentScreen_];

    auto stream = new StdStringStream(message);

    switch (currentScreen_) {
    case 0:
        display.setFont(&TAHOMA_REGULAR_FONT10PT7B);
        break;
    case 1:
        display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
        break;
    case 2:
        display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
        break;
    case 3:
        display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        break;
    case 4:
        display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        break;
    }
    renderer_.setStream(stream);
    renderer_.setHorizontalCenter(true);
    renderer_.setVerticalCenter(true);

    this->renderer_.renderPage(0);

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
        currentScreen_++;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        currentScreen_++;
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        currentScreen_++;
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        currentScreen_++;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
        break;
    default:
        break;
    }

    if (currentScreen_ >= NUM_DEMO_SCREENS) {
        backCallback();
    }
}
