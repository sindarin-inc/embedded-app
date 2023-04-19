#include "UI/FirmwareUpdateScreen.hpp"

#include <esp_system.h>

#include "Communications/FirmwareDownload.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/PowerManager.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "sindarin-debug.h"

void FirmwareUpdateScreen::render(DisplaySystem &display) {
    uint16_t x = 0;
    uint16_t y = 0;

    // Re-render based on state
    switch (state_.state) {
    case FirmwareDownloadState::Idle: {
        // Shouldn't get here
        log_w("FirmwareUpdateScreen::render() called in Idle state");
        return;
    }
    case FirmwareDownloadState::Failure: {
        display.drawSingleLineOfCenteredText(state_.message);
        break;
    }
    case FirmwareDownloadState::ConnectingWifi:
    case FirmwareDownloadState::CheckingForLatest:
    case FirmwareDownloadState::AlreadyUpToDate:
    case FirmwareDownloadState::Downloading:
    case FirmwareDownloadState::AlreadyDownloaded:
    case FirmwareDownloadState::Downloaded:
    case FirmwareDownloadState::Installing:
    case FirmwareDownloadState::Installed: {
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
                       state_.state > FirmwareDownloadState::ConnectingWifi ? 100 : 0, false);

        drawBulletLine("Checking for Update",
                       state_.state > FirmwareDownloadState::CheckingForLatest ? 100 : 0, false);

        // We don't know the rest of the list until this step is done, bail out
        if (state_.state <= FirmwareDownloadState::CheckingForLatest) {
            break;
        }

        if (state_.state == FirmwareDownloadState::AlreadyUpToDate) {
            drawBulletLine(
                StringFormat("Already up to date! (%s)", firmwareDownloader_.versionString.c_str())
                    .c_str(),
                100, false);
            break;
        }

        auto downloadPercentage =
            state_.state < FirmwareDownloadState::Downloading ? 0 : state_.downloadPercentage;
        if (state_.state > FirmwareDownloadState::Downloading) {
            downloadPercentage = 100;
        }

        drawBulletLine(
            StringFormat("Downloading Update %s", firmwareDownloader_.versionString.c_str())
                .c_str(),
            downloadPercentage, true);

        drawBulletLine("Installing", state_.state > FirmwareDownloadState::Installing ? 100 : 0,
                       false);

        drawBulletLine("Rebooting...", state_.state == FirmwareDownloadState::Installed ? 100 : 0,
                       false);
        break;
    }
    }
    // Full partial for state change otherwise AU4
    if (needsFullUpdate_) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        needsFullUpdate_ = false;
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    }
}

void FirmwareUpdateScreen::processCommand(RemoteCommand command) {}

void FirmwareUpdateScreen::fail(const std::string &message) {
    log_w("Firmware update failed: %s", message.c_str());
    state_.state = FirmwareDownloadState::Failure;
    state_.message = message;
    setNeedsRerender();
    render(display_);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    WiFi::getInstance().disconnectWiFi();
    backCallback();
}

void FirmwareUpdateScreen::begin() {
    if (!WiFi::getInstance().hasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

    state_.state = FirmwareDownloadState::ConnectingWifi;
    render(display_);

    if (!WiFi::getInstance().connectWiFiWithTimeout()) {
        fail("Failed to connect to WiFi.");
        return;
    }

    firmwareDownloader_.downloadFirmware([this](const FirmwareDownloadState &state) {
        DispatchGetUiQueue()->dispatch([this, state]() {
            if (state_ == state) {
                log_d("State unchanged, skipping update");
                return;
            }
            log_d("Updating from state %d to %d (%d%%)", state_.state, state.state,
                  state.downloadPercentage);
            SHOW_HEAP();
            updateState(state);
            this->render(this->display_);

            if (state.state == FirmwareDownloadState::Failure ||
                state.state == FirmwareDownloadState::Installing ||
                state.state == FirmwareDownloadState::AlreadyUpToDate) {
                WiFi::getInstance().disconnectWiFi();
            }

            if (state.state == FirmwareDownloadState::Failure ||
                state.state == FirmwareDownloadState::AlreadyUpToDate) {
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                backCallback();
            } else if (state.state == FirmwareDownloadState::Installed) {
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                log_i("Rebooting after successful firmware update");
                PowerManager::getInstance().reboot();
                // Only for the sim
                backCallback();
            }
        });
    });
}
