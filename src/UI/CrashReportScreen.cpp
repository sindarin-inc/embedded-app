#include "UI/CrashReportScreen.hpp"

#include "Communications/CrashReportUploader.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Dispatch.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "sindarin-debug.h"

void CrashReportScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    uint16_t x = 0;
    uint16_t y = 0;

    // Re-render based on state
    switch (state_.state) {
    case CrashUploadState::Idle: {
        // Shouldn't get here
        log_w("FirmwareUpdateScreen::render() called in Idle state");
        return;
    }
    case CrashUploadState::Failure: {
        display.drawSingleLineOfCenteredText(state_.message);
        break;
    }
    case CrashUploadState::ConnectingWifi:
    case CrashUploadState::Uploading:
    case CrashUploadState::Done: {
        // Calculate group height
        const uint16_t spacing = 2;
        const uint16_t lineCount = 5;
        uint16_t textHeight = display.getFont()->yAdvance;
        uint16_t groupHeight = textHeight * lineCount + spacing * (lineCount - 1);

        y = (display.height() - groupHeight) / 2;

        auto drawBulletLine = [&display, textHeight, &x, &y](const char *message, int percentage,
                                                             bool drawLine) {
            uint16_t textWidth = display.getTextWidth(message);
            uint16_t bulletRadius = textHeight / 3;
            uint16_t bulletY = y + (textHeight - bulletRadius * 2) / 2 + bulletRadius;
            if (percentage < 100) {
                display.drawCircle(x + bulletRadius, bulletY, bulletRadius, display.fg);
            } else {
                display.fillCircle(x + bulletRadius, bulletY, bulletRadius, display.fg);
            }
            auto textX = x + bulletRadius * 2 + 5;
            display.drawSingleLineOfText(message, textX, y);
            y += display.getFont()->yAdvance;
            if (drawLine && percentage > 0) {
                display.drawLine(textX, y, textX + (textWidth * percentage / 100), y, display.fg);
            }
            y += spacing;
        };

        drawBulletLine("Connecting WiFi",
                       state_.state > CrashUploadState::ConnectingWifi ? 100 : 0, false);

        drawBulletLine("Checking for Update",
                       state_.state > CrashUploadState::CheckingForCrashReport ? 100 : 0, false);

        // We don't know the rest of the list until this step is done, bail out
        if (state_.state <= CrashUploadState::CheckingForCrashReport) {
            break;
        }

        /*
        auto downloadPercentage =
            state_.state < FirmwareDownloadState::Downloading ? 0 : state_.downloadPercentage;
        if (state_.state > FirmwareDownloadState::Downloading) {
            downloadPercentage = 100;
        }

        drawBulletLine(
            StringFormat("Downloading Update %s", firmwareDownloader_.versionString.c_str())
                .c_str(),
            downloadPercentage, true);
        */

        drawBulletLine(
            "Uploading", state_.state > CrashUploadState::Uploading ? 100 : 0, false);

        /*
        drawBulletLine("Installing", state_.state > FirmwareDownloadState::Installing ? 100 : 0,
                       false);

        drawBulletLine("Rebooting...", state_.state == FirmwareDownloadState::Installed ? 100 : 0,
                       false);
                       */
        break;
    }
    }
    // Full partial for state change otherwise AU4
    if (needsFullUpdate_) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        needsFullUpdate_ = false;
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::FAST);
    }

    needsRerender = false;
}

void CrashReportScreen::processCommand(RemoteCommand command) {}

void CrashReportScreen::fail(const std::string &message) {
    log_w("Firmware update failed: %s", message.c_str());
    state_.state = CrashUploadState::Failure;
    state_.message = message;
    needsRerender = true;
    render(display_);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    DisconnectWifi();
    backCallback();
}

void CrashReportScreen::begin() {
    if (!HasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

    state_.state = CrashUploadState::ConnectingWifi;
    render(display_);

    if (!ConnectWifi()) {
        fail("Failed to connect to WiFi.");
        return;
    }

    log_i("I'm here!");

    crashReportUploader_.uploadCrash([this](const CrashUploadState &state) {
        DispatchGetUiQueue()->dispatch([this, state]() {
            //log_d("Updating from state %d to %d (%d%%)", state_.state, state.state,
                  //state.downloadPercentage);
            log_d("Updating from state %d to %d", state_.state, state.state);
            updateState(state);
            this->render(this->display_);

            if (state.state == CrashUploadState::Failure ||
                state.state == CrashUploadState::NoCrashReport) {
                DisconnectWifi();
            }

            if (state.state == CrashUploadState::Failure ||
                state.state == CrashUploadState::NoCrashReport) {
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                backCallback();
            } else if (state.state == CrashUploadState::Done) {
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                log_i("Rebooting after successful firmware update");
                // Only for the sim
                backCallback();
            }
        });
    });
}
