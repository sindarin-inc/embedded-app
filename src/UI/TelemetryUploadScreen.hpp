#pragma once

#include <string>
#include <utility>

#include "Misc/Telemetry.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

struct TelemetryUploadState {
    enum State {
        Idle,
        ConnectingWifi,
        Uploading,
        Done,
        Failure,
    } state = Idle;
    std::string message;
};

class TelemetryUploadScreen : public Screen {
public:
    TelemetryUploadScreen(DisplaySystem &display, PreferencesStore &prefs, FileSystem &fileSystem,
                          Telemetry &telemetry)
        : display_(display), prefs_(prefs), fileSystem_(fileSystem), telemetry_(telemetry){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    FileSystem &fileSystem_;
    Telemetry &telemetry_;

    bool needsFullUpdate_ = true;
    TelemetryUploadState state_;

    void fail(const std::string &message);
    void updateState(const TelemetryUploadState &state) {
        if (state.state == TelemetryUploadState::Failure) {
            needsFullUpdate_ = true;
        }
        state_ = state;
        setNeedsRerender();
    }
};
