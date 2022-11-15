#pragma once

#include <string>
#include <utility>

#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

struct VoltageLogUploadState {
    enum State {
        Idle,
        ConnectingWifi,
        Uploading,
        Done,
        Failure,
    } state = Idle;
    std::string message;
};

class VoltageLogUploadScreen : public Screen {
public:
    VoltageLogUploadScreen(DisplaySystem &display, PreferencesStore &prefs, FileSystem &fileSystem)
        : display_(display), prefs_(prefs), fileSystem_(fileSystem){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    FileSystem &fileSystem_;
    bool needsFullUpdate_ = true;
    VoltageLogUploadState state_;

    void fail(const std::string &message);
    void updateState(const VoltageLogUploadState &state) {
        if (state.state == VoltageLogUploadState::Failure) {
            needsFullUpdate_ = true;
        }
        state_ = state;
        needsRerender = true;
    }
};
