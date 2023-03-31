#include "VoltageLogUploadScreen.hpp"

#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Dispatch.hpp"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "sindarin-debug.h"

void VoltageLogUploadScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    uint16_t x = 0;
    uint16_t y = 0;

    // Re-render based on state
    switch (state_.state) {
    case VoltageLogUploadState::Idle: {
        // Shouldn't get here
        log_w("VoltageLogUploadScreen::render() called in Idle state");
        return;
    }
    case VoltageLogUploadState::Failure: {
        display.drawSingleLineOfCenteredText(state_.message);
        break;
    }
    case VoltageLogUploadState::ConnectingWifi:
    case VoltageLogUploadState::Uploading:
    case VoltageLogUploadState::Done: {
        // Calculate group height
        const uint16_t spacing = 2;
        const uint16_t lineCount = 3;
        uint16_t textHeight = display.getFont()->yAdvance;
        uint16_t groupHeight = textHeight * lineCount + spacing * (lineCount - 1);

        y = (display.height() - groupHeight) / 2;

        auto drawBulletLine = [&display, textHeight, &x, &y](const char *message, bool fillBullet) {
            uint16_t bulletRadius = textHeight / 3;
            uint16_t bulletY = y + (textHeight - bulletRadius * 2) / 2 + bulletRadius;
            if (fillBullet) {
                display.fillCircle(x + bulletRadius, bulletY, bulletRadius, display.fg);
            } else {
                display.drawCircle(x + bulletRadius, bulletY, bulletRadius, display.fg);
            }
            auto textX = x + bulletRadius * 2 + 5;
            display.drawSingleLineOfText(message, textX, y);
            y += display.getFont()->yAdvance;
            y += spacing;
        };

        drawBulletLine("Connecting WiFi", state_.state > VoltageLogUploadState::ConnectingWifi);
        drawBulletLine("Uploading voltage log", state_.state > VoltageLogUploadState::Uploading);
        drawBulletLine("Deleting local copy", state_.state > VoltageLogUploadState::Uploading);

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

void VoltageLogUploadScreen::processCommand(RemoteCommand command) {}

void VoltageLogUploadScreen::fail(const std::string &message) {
    log_w("Voltage log upload failed: %s", message.c_str());
    state_.state = VoltageLogUploadState::Failure;
    state_.message = message;
    updateState(state_);
    render(display_);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    WiFi::getInstance().disconnectWiFi();
    backCallback();
}

void VoltageLogUploadScreen::begin() {
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        fail("Go to Settings > Pair Glasses in the Sol app.");
        return;
    }
    SindarinAPI::getInstance()->setAccessToken(token.value());

    if (!WiFi::getInstance().hasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

    state_.state = VoltageLogUploadState::ConnectingWifi;
    render(display_);

    DispatchGetCore1BackgroundQueue()->dispatch([this]() {
        if (!WiFi::getInstance().connectWiFiWithTimeout()) {
            DispatchGetUiQueue()->dispatch([this]() { fail("Failed to connect to WiFi."); });
            return;
        }

        state_.state = VoltageLogUploadState::Uploading;
        updateState(state_);
        DispatchGetUiQueue()->dispatch([this]() { render(display_); });

        auto httpCode = SindarinAPI::getInstance()->uploadAndResetVoltageLog(fileSystem_);
        if (httpCode != 201) {
            DispatchGetUiQueue()->dispatch([this, httpCode]() {
                fail(StringFormat("Failed to upload voltage log (%d).", httpCode));
            });
            return;
        }

        log_d("Deleting voltage log");
        fileSystem_.remove(Battery::voltageLogPath());

        DispatchGetUiQueue()->dispatch([this]() {
            log_i("Voltage log upload complete");
            state_.state = VoltageLogUploadState::Done;
            updateState(state_);
            render(display_);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            WiFi::getInstance().disconnectWiFi();
            backCallback();
        });

        return;
    });
}
