#pragma once

#include <string>
#include <utility>

#include "Communications/FirmwareDownload.hpp"
#include "Communications/SindarinWiFi.hpp"
#include "UI/Screen.hpp"

class FirmwareUpdateScreen : public Screen {
public:
    FirmwareUpdateScreen(DisplaySystem &display, PreferencesStore &prefs)
        : display_(display), prefs_(prefs), firmwareDownloader_(prefs){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    FirmwareDownloader firmwareDownloader_;
    bool needsFullUpdate_ = true;

    FirmwareDownloadState state_;

    void fail(const std::string &message);
    void updateState(const FirmwareDownloadState &state) {
        // After `CheckingForLatest` we will change the list significantly
        // Also if we fail, we do a full-screen takeover
        if (state_.state == FirmwareDownloadState::CheckingForLatest ||
            state.state == FirmwareDownloadState::Failure) {
            needsFullUpdate_ = true;
        }
        state_ = state;
        needsRerender = true;
    }
};
