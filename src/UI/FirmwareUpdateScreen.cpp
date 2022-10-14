#include "UI/FirmwareUpdateScreen.hpp"

#include "Communications/FirmwareDownload.hpp"
#include "Communications/WiFi.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "sindarin-debug.h"

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

        // Let other tasks do stuff. Particularly important while we have this on the UI thread
        // TODO: Don't do this on the UI thread
        vPortYield();

        if (error) {
            std::string errMessage = "Error updating firmware.";
            drawMessage(errMessage);
            done = true;
        } else if (done) {
            std::string errMessage = "Done! Rebooting...";
            drawMessage(errMessage);
            log_i("Rebooting in 2s to pick up new firmware");
            delay(2000);
            esp_restart();
        } else if (percentProgress - lastPercentage >= 5) {
            lastPercentage = percentProgress;
            char formattedString[32];
            // NOLINTNEXTLINE(cert-err33-c) -- this is a safe, fixed length, format string
            snprintf(formattedString, sizeof(formattedString), "%02d %%", percentProgress);
            std::string updateMessage = "Updating: ";
            updateMessage += formattedString;
            log_i("Updating firmware: %02d%%", percentProgress);
            drawMessage(updateMessage);
        }

        vPortYield();
    }

    log_i("done firmware update.");
}

void FirmwareUpdateScreen::drawMessage(std::string &message) {
    display_.drawSingleLineOfCenteredText(message);
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void FirmwareUpdateScreen::processCommand(RemoteCommand command) { backCallback(); }
