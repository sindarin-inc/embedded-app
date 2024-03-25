#pragma once

#include "Misc/Telemetry.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OnboardingContentSyncScreen : public Screen {
public:
    OnboardingContentSyncScreen(DisplaySystem &display, LocalStore &localStore,
                                PreferencesStore &prefs, Telemetry &telemetry)
        : display_(display), localStore_(localStore), prefs_(prefs), telemetry_(telemetry),
          onboardingSyncRenderer_(display){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    LocalStore &localStore_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    ContentSyncState lastState_;
    ContentSyncState state_;

    Renderer onboardingSyncRenderer_;

    bool shownOnboardingSyncMessage_ = false;

    void fail(const std::string &message);
    void updateState(const ContentSyncState &state) {
        lastState_ = state_;
        state_ = state;
        setNeedsRerender();
    }
};
