#include "ContentSyncScreen.hpp"

#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Renderers/RendererStream.hpp"

#if USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
extern TTRemoteInput ttRemoteInput;
#endif

void ContentSyncScreen::render(DisplaySystem &display) {
    uint16_t x = 0;
    uint16_t y = 0;

    if (onboardingSync_) {
        if (!shownOnboardingSyncMessage_) {
            // We need to make sure this goes onto the second page because of a bug in the renderer.
            std::string message;

            if (!WiFi::getInstance().hasWiFiCredentials()) {
                message =
                    "Looks like you didn't set up the mobile app before you got here.\n\n"
                    "Go to solreader.com/app and download the Sol Reader app to get connected to "
                    "WiFi so we can download your content.\n\n"
                    "Don't feel like it right now? No worries! You can browse around the menus and "
                    "read a couple of tutorials.\n\n\nPress the \ue000 button to continue.";
            } else {
                message =
                    "Give us 2-4 minutes to sync your content for the first time. Take "
                    "off your Sol Reader and close the temples.\n\nMaybe go "
                    "for a walk. Listen to the birds. Dare you leave "
                    "your phone at home?\n\n When you're back, if the displays are dark "
                    "and the Blue LED is off, open the temples. You're ready to read...\n\nOf "
                    "course, "
                    "if you want to stare anxiously at the Blue LED in anticipation, by all means. "
                    "It's exciting!\n\n";
            }

            auto streamFirstTime = std::make_shared<StdStringStream>(message);
            onboardingSyncRenderer_.setStream(streamFirstTime);
            display.setFont(static_cast<Font *>(&ibmf75Face0));
            onboardingSyncRenderer_.renderPage(0);
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
            shownOnboardingSyncMessage_ = true;

#if USE_TURN_TOUCH_REMOTE
            if (WiFi::getInstance().hasWiFiCredentials()) {
                // Disconnect the remote so it connects more quickly on reboot
                ttRemoteInput.disconnectTemporarily(1000);
            }
#endif
        }
        // Only render once in onboardingSync_ mode
        return;
    }
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

            std::string truncatedMessage;
            if (std::strlen(message) > 32) {
                truncatedMessage = std::string(message).substr(0, 32) + "...";
                message = truncatedMessage.c_str();
                textWidth = display.getTextWidth(message);
            }

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

void ContentSyncScreen::processCommand(RemoteCommand command) {
    if ((command == RemoteCommand::ENTER) && !WiFi::getInstance().hasWiFiCredentials()) {
        back();
    }
};

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
    // We're just going to wait until they press a button in this case
    if (onboardingSync_ && !WiFi::getInstance().hasWiFiCredentials()) {
        return;
    }

    // If we're in first time mode, we need to make sure we have a cloud token and WiFi
    // credentials before we can do anything.
    if (!prefs_.cloudToken.get().has_value()) {
        fail("Go to Settings > Pair Glasses in the Sol app.");
        return;
    }

    if (!WiFi::getInstance().hasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

    if (onboardingSync_) {
        PowerManager::getInstance().forceOn();
        DigitalWrite(BLUE_LED, LED_ON);
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
                this->display_.clearDisplay();
                this->display_.clearFrameBuffer();
                this->display_.updateDisplay();
                PowerManager::getInstance().reboot();
            }
#endif
            if (state.state == ContentSyncState::Failure ||
                state.state == ContentSyncState::Synced ||
                state.state == ContentSyncState::AlreadySynced) {
#if !SIMULATOR
                if (onboardingSync_) {
                    // Todo: maybe we want to retry once?
                    this->display_.clearDisplay();
                    this->display_.clearFrameBuffer();
                    this->display_.updateDisplay();
                    PowerManager::getInstance().reboot();
                } else
#endif
                {
                    log_d("Content sync done, waiting 5 seconds before going back");
                    Delay(5000);
                    back();
                }
            }
        });
    });
}
