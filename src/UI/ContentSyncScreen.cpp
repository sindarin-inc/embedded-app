#include "ContentSyncScreen.hpp"

#include "Communications/WiFi.hpp"

void ContentSyncScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    uint16_t x = 0;
    uint16_t y = 0;

    // Re-render based on state
    switch (state_.state) {
    case ContentSyncState::Idle: {
        // Shouldn't get here
        log_w("ContentSyncScreen::render() called in Idle state");
        return;
    }
    case ContentSyncState::ConnectingWifi: {
        display.drawSingleLineOfCenteredText("Connecting WiFi...");
        break;
    }
    case ContentSyncState::CheckingSync: {
        display.drawSingleLineOfCenteredText("Checking sync...");
        break;
    }
    case ContentSyncState::LoadingTasks: {
        display.drawSingleLineOfCenteredText("Loading tasks...");
        break;
    }
    case ContentSyncState::Syncing: {
        uint16_t textHeight = display.getFont()->yAdvance;

        // Calculate group height
        const uint16_t spacing = 2;
        uint16_t groupHeight =
            textHeight * state_.tasks.size() + spacing * (state_.tasks.size() - 1);

        y = (display.height() - groupHeight) / 2;

        // Draw list of tasks
        for (const auto &task : state_.tasks) {
            uint16_t textWidth = display.getTextWidth(task.message.c_str());
            uint16_t bulletRadius = textHeight / 3;
            uint16_t bulletY = y + (textHeight - bulletRadius * 2) / 2 + bulletRadius;
            if (task.percentage < 100) {
                display.drawCircle(x + bulletRadius, bulletY, bulletRadius, display.fg);
            } else {
                display.fillCircle(x + bulletRadius, bulletY, bulletRadius, display.fg);
            }
            auto textX = x + bulletRadius * 2 + 5;
            display.drawSingleLineOfText(task.message.c_str(), textX, y);
            y += display.getFont()->yAdvance;
            if (task.percentage > 0) {
                display.drawLine(textX, y, textX + (textWidth * task.percentage / 100), y,
                                 display.fg);
            }
            y += spacing;
        }
        break;
    }
    case ContentSyncState::Synced: {
        display.drawSingleLineOfCenteredText("Done! Restarting...");
        break;
    }
    case ContentSyncState::AlreadySynced: {
        display.drawSingleLineOfCenteredText("No new books or articles.");
        break;
    }
    case ContentSyncState::Failure: {
        display.drawSingleLineOfCenteredText(state_.message);
        break;
    }
    }
    // Full partial for state change otherwise AU4
    if (lastState_.state != state_.state) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    }

    needsRerender = false;
}

void ContentSyncScreen::processCommand(RemoteCommand command){};

void ContentSyncScreen::fail(const std::string &message) {
    log_w("Content sync failed: %s", message.c_str());
    state_.state = ContentSyncState::Failure;
    state_.message = message;
    needsRerender = true;
    render(display_);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    DisconnectWifi();
    if (backCallback) {
        backCallback();
    } else {
        log_w("No back callback set for ContentSyncScreen");
    }
}

void ContentSyncScreen::begin() {
    if (!prefs_.cloudToken.get().has_value()) {
        fail("Go to Settings > Pair Glasses in the Sol app.");
        return;
    }

    if (!HasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

    state_.state = ContentSyncState::ConnectingWifi;
    render(display_);

    localStore_.syncFromRemote([this](const ContentSyncState &state) {
        if (state.state == ContentSyncState::Failure || state.state == ContentSyncState::Synced ||
            state.state == ContentSyncState::AlreadySynced) {
            DisconnectWifi();
        }
        // Do UI stuff back on core 1
        DispatchGetUiQueue()->dispatch([this, state]() {
            this->updateState(state);
            this->render(this->display_);
#if !SIMULATOR
            if (state.state == ContentSyncState::Synced) {
                log_d("Content sync done, rebooting");
                esp_restart();
            }
#endif
            if (state.state == ContentSyncState::Failure ||
                state.state == ContentSyncState::Synced ||
                state.state == ContentSyncState::AlreadySynced) {
                delay(5000);
                backCallback();
            }
        });
    });
}
