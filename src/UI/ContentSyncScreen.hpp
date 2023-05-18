#pragma once

#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ContentSyncScreen : public Screen {
public:
    ContentSyncScreen(DisplaySystem &display, LocalStore &localStore, PreferencesStore &prefs,
                      bool firstTime = false)
        : display_(display), localStore_(localStore), prefs_(prefs), firstTime_(firstTime){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    LocalStore &localStore_;
    PreferencesStore &prefs_;
    ContentSyncState lastState_;
    ContentSyncState state_;

    bool firstTime_ = false;
    bool shownFirstTime_ = false;

    void fail(const std::string &message);
    void updateState(const ContentSyncState &state) {
        lastState_ = state_;
        state_ = state;
        setNeedsRerender();
    }
};
