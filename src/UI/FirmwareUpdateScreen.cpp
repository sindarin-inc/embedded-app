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

    // don't need additional rendering after update completes
    // or an error occurs
    needsRerender = false;

    // first check if wifi is connected
    std::string message = "No Wifi Connection";
    if (NetworkConnected()) {
        message = "Updating: 00%";
        if (!firmwareDownload_.getFirmware()) {
            message = "Error updating firmware.";
            drawMessage(message);
            return;
        }
    } else {
        drawMessage(message);
        return;
    }

    drawMessage(message);

    bool done = false;
    int lastPercentage = 0;
    while (!done) {
        int percentProgress;
        bool error = false;
        done = firmwareDownload_.updateFirmware(percentProgress, error);

        if (error) {
            std::string errMessage = "Error updating firmware.";
            drawMessage(errMessage);
            done = true;
        } else if (percentProgress - lastPercentage >= 5) {
            lastPercentage = percentProgress;
            char formattedString[32];
            sprintf(formattedString, "%02d %%", percentProgress);
            std::string updateMessage = "Updating: ";
            updateMessage += formattedString;
            drawMessage(updateMessage);
        }
    }

    log_i("done firmware update.");
}

void FirmwareUpdateScreen::drawMessage(std::string &message) {
    display_.setFont(&TAHOMA_REGULAR_FONT10PT7B);
    display_.drawSingleLineOfText(message, 0, 0);
    display_.updateDisplay();
}

void FirmwareUpdateScreen::processCommand(RemoteCommand command) { backCallback(); }
