#include "ContentSyncScreen.hpp"

#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"

void ContentSyncScreen::render(DisplaySystem &display) {
    uint16_t x = 0;
    uint16_t y = 0;

    display_.setFont(display_.style.getFontFamily().settingsFont);

    // Re-render based on state
    switch (state_.state) {
    case ContentSyncState::Idle: {
        // Shouldn't get here
        log_w("ContentSyncScreen::render() called in Idle state");
        return;
    }

        { break; }
    case ContentSyncState::ConnectingWifi:
    case ContentSyncState::CheckingSync:
    case ContentSyncState::SyncingBooks:
    case ContentSyncState::SyncingArticles:
    case ContentSyncState::Syncing:
    case ContentSyncState::AlreadySynced:
    case ContentSyncState::Synced: {
        // There's no way to know where to set the Y, so we offset from the top a bit, and if it
        // runs off the page, so be it. Realistically, this only happens if the user has a ton of
        // things to sync (e.g., 6+ books).
        const uint16_t spacing = 2;
        uint16_t textHeight = display.getFont()->lineHeight();
        uint16_t lineCount;

        if (state_.state <= ContentSyncState::SyncingArticles) {
            lineCount = 4;
        } else {
            lineCount = state_.tasks.size();
        }

        if (lineCount == 0) {
            lineCount = 1;
        }

        uint16_t groupHeight = textHeight * lineCount + spacing * (lineCount - 1);

        y = (display.height() - groupHeight) / 2;

        auto drawBulletLine =
            [&display, textHeight, &x, &y](const char *message, int percentage, bool drawLine) {
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
            y += display.getFont()->lineHeight();
            if (drawLine && percentage > 0) {
                display.drawLine(textX, y, textX + (textWidth * percentage / 100), y, display.fg);
            }
            y += spacing;
        };

        if (state_.state <= ContentSyncState::SyncingArticles) {

            drawBulletLine("Connecting WiFi",
                           state_.state > ContentSyncState::ConnectingWifi ? 100 : 0, false);

            drawBulletLine("Checking for update",
                           state_.state > ContentSyncState::CheckingSync ? 100 : 0, false);

            drawBulletLine("Syncing books...",
                           state_.state > ContentSyncState::SyncingBooks ? 100 : 0, false);

            drawBulletLine("Syncing articles...",
                           state_.state > ContentSyncState::SyncingArticles ? 100 : 0, false);

            break;
        }
        for (const auto &task : state_.tasks) {
            drawBulletLine(task.message.c_str(), task.percentage, true);
        }

        if (state_.state == ContentSyncState::AlreadySynced) {
            drawBulletLine("No new books or articles", 100, false);
            break;
        }

        // The tasks are already gone by this point, so it's hard to redraw them. Just
        // replace where they are drawn for now.
        if (state_.state == ContentSyncState::Synced) {
            drawBulletLine("Done! Restarting...",
                           state_.state == ContentSyncState::Synced ? 100 : 0, false);
        }
        break;
    }

    case ContentSyncState::Failure: {
        display.drawSingleLineOfCenteredText(state_.message);
        break;
    }
    }
    // Full partial for state change otherwise AU4
    if ((lastState_.state != state_.state) || (state_.state == ContentSyncState::AlreadySynced) ||
        (lastState_.tasks.size() != state_.tasks.size())) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    }
}

void ContentSyncScreen::processCommand(RemoteCommand command){};

void ContentSyncScreen::fail(const std::string &message) {
    log_w("Content sync failed: %s", message.c_str());
    state_.state = ContentSyncState::Failure;
    state_.message = message;
    setNeedsRerender();
    render(display_);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    WiFi::getInstance().disconnectWiFi();
#if !SIMULATOR
    PowerManager::getInstance().reboot();
#endif
    back();
}

void ContentSyncScreen::begin() {
    if (!prefs_.cloudToken.get().has_value()) {
        fail("Go to Settings > Pair Glasses in the Sol app.");
        return;
    }

    if (!WiFi::getInstance().hasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

    state_.state = ContentSyncState::ConnectingWifi;
    render(display_);

    localStore_.syncFromRemote([this](const ContentSyncState &state) {
        PowerManager::getInstance().feedInactivityTimer();

        if (state.state == ContentSyncState::Failure || state.state == ContentSyncState::Synced ||
            state.state == ContentSyncState::AlreadySynced) {
            WiFi::getInstance().disconnectWiFi();
        }
        // Do UI stuff back on core 1
        DispatchGetUiQueue()->dispatch([this, state]() {
            this->updateState(state);
            this->render(this->display_);
#if !SIMULATOR
            if (state.state == ContentSyncState::Synced) {
                log_d("Content sync done, rebooting");
                PowerManager::getInstance().reboot();
            }
#endif
            if (state.state == ContentSyncState::Failure ||
                state.state == ContentSyncState::Synced ||
                state.state == ContentSyncState::AlreadySynced) {
                log_d("Content sync done, waiting 5 seconds before going back");
                Delay(5000);
                back();
            }
        });
    });
}
