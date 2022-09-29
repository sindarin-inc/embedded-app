#include "UI/FirmwareUpdateScreen.hpp"

#include "Communications/FirmwareDownload.hpp"
#include "Communications/WiFi.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "sindarin-debug.h"

FirmwareUpdateScreen::FirmwareUpdateScreen(DisplaySystem &display) : display_(display) {}

void FirmwareUpdateScreen::render(DisplaySystem &display) {

    if (!needsRerender) {
        return;
    }

    std::string message = "No Wifi";
    if (NetworkConnected()) {
        message = "Updating: 00%";
        firmwareDownload_.GetFirmware();
    } else {
        return;
    }

    display.setFont(&TAHOMA_REGULAR_FONT10PT7B);
    display.drawSingleLineOfText(message, 0, 0);
    display.updateDisplay();

    bool done = false;
    int lastPercentage = 0;
    while (!done) {
        int percentProgress;
        bool error = false;
        done = firmwareDownload_.UpdateFirmware(percentProgress, error);

        if (error)
        {
            std::string errMessage = "Error updating firmware.";
            display.setFont(&TAHOMA_REGULAR_FONT10PT7B);
            display.drawSingleLineOfText(errMessage, 0, 0);
            display.updateDisplay();
            done = true;
        }
        else if (percentProgress - lastPercentage >= 5)
        {
            lastPercentage = percentProgress;
            char formattedString[32];
            sprintf(formattedString, "%02d %%", percentProgress);
            std::string updateMessage = "Updating: ";
            updateMessage += formattedString;
            display.setFont(&TAHOMA_REGULAR_FONT10PT7B);
            display.drawSingleLineOfText(updateMessage, 0, 0);
            display.updateDisplay();
        }
    }

    log_i("done firmware update.");
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
    log_i("firmware update command!");
    switch (command) {
    case RemoteCommand::BACK: {
        backCallback();
        break;
    }
    case RemoteCommand::ENTER: {
        backCallback();
        break;
    }
    default: {
        backCallback();
        break;
    }
    }
}
