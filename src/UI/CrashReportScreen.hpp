#pragma once

#include <string>
#include <utility>

#include "Communications/CrashReportUploader.hpp"
#include "Communications/WiFi.hpp"
#include "UI/Screen.hpp"

class CrashReportScreen : public Screen {
public:
    CrashReportScreen(DisplaySystem &display, FileSystem &fileSystem)
        : display_(display), crashReportUploader_(fileSystem){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    CrashUploader crashReportUploader_;
    bool needsFullUpdate_ = true;

    CrashUploadState state_;

    void fail(const std::string &message);
    void updateState(const CrashUploadState &state) {
        if (state_.state == CrashUploadState::CheckingForCrashReport ||
            state.state == CrashUploadState::Failure) {
            needsFullUpdate_ = true;
        }
        state_ = state;
        needsRerender = true;
    }
};
