#pragma once

#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ContentSyncScreen : public Screen {
public:
    ContentSyncScreen(DisplaySystem &display, LocalStore &localStore, PreferencesStore &prefs,
                      bool onboardingSync = false)
        : display_(display), localStore_(localStore), prefs_(prefs),
          onboardingSync_(onboardingSync), onboardingSyncRenderer_(display){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    LocalStore &localStore_;
    PreferencesStore &prefs_;
    ContentSyncState lastState_;
    ContentSyncState state_;

    bool onboardingSync_ = false;

    Renderer onboardingSyncRenderer_;

    bool shownOnboardingSyncMessage_ = false;

    void fail(const std::string &message);
    void updateState(const ContentSyncState &state) {
        lastState_ = state_;
        state_ = state;
        setNeedsRerender();
    }
};
