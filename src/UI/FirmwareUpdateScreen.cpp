#include "UI/FirmwareUpdateScreen.hpp"
#include "Communications/WiFi.hpp"
#include "Communications/FirmwareDownload.hpp"

#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "sindarin-debug.h"

FirmwareUpdateScreen::FirmwareUpdateScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {}

void FirmwareUpdateScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    std::string message = "No Wifi";
    if (NetworkConnected()) {
        message = "Wifi Connected, downloading.";

        firmwareDownload_.GetFirmware();
    }

    auto stream = new StdStringStream(message);
    display.setFont(&TAHOMA_REGULAR_FONT8PT7B);

    renderer_.setStream(stream);
    renderer_.setHorizontalCenter(true);
    renderer_.setVerticalCenter(true);

    this->renderer_.renderPage(0);

    needsRerender = false;
}

void FirmwareUpdateScreen::processCommand(RemoteCommand command) {
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
        backCallback();
        break;
    case RemoteCommand::ENTER:
        break;
    default:
        break;
    }
}
