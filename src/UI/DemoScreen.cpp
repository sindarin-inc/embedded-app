#include "UI/DemoScreen.hpp"

#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 3

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {}

void DemoScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    std::string message = messages[currentScreen_];

    auto stream = new StdStringStream(message);

    switch (currentScreen_) {
    case 0:
        display.setFont(&TAHOMA_REGULAR_FONT10PT7B);
        break;
    case 1:
        display.setFont(&Tahoma_Regular_font8pt7b);
        break;
    case 2:
        display.setFont(&Tahoma_Regular_font7pt7b);
        break;
    case 3:
        display.setFont(&TAHOMA_REGULAR_FONT6PT7B);
        break;
    case 4:
        display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
        break;
    }
    renderer_.setStream(stream);

    this->renderer_.renderPage(0);

    needsRerender = false;
}

void DemoScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        needsRerender = true;
        break;
    case RemoteCommand::BACK:

    case RemoteCommand::ENTER:

        break;
    default:
        break;
    }
    currentScreen_++;
    if (currentScreen_ >= NUM_DEMO_SCREENS) {
        backCallback();
    }
}
