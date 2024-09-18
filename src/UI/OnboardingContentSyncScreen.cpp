#include "OnboardingContentSyncScreen.hpp"

#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Renderers/RendererStream.hpp"
#include "UI/Fonts/Fonts.hpp"

#if CONFIG_USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
extern TTRemoteInput ttRemoteInput;
#endif

void OnboardingContentSyncScreen::render(DisplaySystem &display) {
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
        display.setFont(static_cast<Font *>(&fontFace0));
        onboardingSyncRenderer_.renderFirstPage();
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
        shownOnboardingSyncMessage_ = true;
    }
}

void OnboardingContentSyncScreen::processCommand(RemoteCommand command) {
    if ((command == RemoteCommand::ENTER) && !WiFi::getInstance().hasWiFiCredentials()) {
        back();
    }
};

void OnboardingContentSyncScreen::fail(const std::string &message) {
    log_w("Content sync failed: %s", message.c_str());

    this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                               {{"message", message}});
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

void OnboardingContentSyncScreen::begin() {
    // We're just going to wait until they press a button in this case
    if (!WiFi::getInstance().hasWiFiCredentials()) {
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

    PowerManager::getInstance().forceOn();
    DigitalWrite(CONFIG_BLUE_LED, LED_ON);

    state_.state = ContentSyncState::ConnectingWifi;
    render(display_);

    localStore_.syncFromRemote(SyncReason::Manual, nullptr, [this](const ContentSyncState &state) {
        PowerManager::getInstance().feedInactivityTimer();

        if (state.state == ContentSyncState::Failure || state.state == ContentSyncState::Synced ||
            state.state == ContentSyncState::AlreadySynced) {
            WiFi::getInstance().disconnectWiFi();
        }
        // Do UI stuff back on core 1
        DispatchGetUiQueue()->dispatch([this, state]() {
            this->updateState(state);
            this->render(this->display_);
            if (state.state == ContentSyncState::Failure ||
                state.state == ContentSyncState::Synced ||
                state.state == ContentSyncState::AlreadySynced) {
                // Todo: maybe we want to retry once?
                PowerManager::getInstance().reboot();
            }
        });
    });
}
