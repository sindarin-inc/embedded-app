#pragma once

#include <HTTPClient.h>
#include <cstdint>
#include <utility>

#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

struct CrashUploadState {
    enum State {
        Idle,
        ConnectingWifi,
        CheckingForCrashReport,
        Uploading,
        Done,
        Failure,
        NoCrashReport,
    } state = Idle;

    int uploadPercentage;
    std::string message;
};

class CrashUploader {
public:
    CrashUploader(FileSystem &fileSystem, PreferencesStore &prefs)
        : fileSystem_(fileSystem), prefs_(prefs){};

    void uploadCrash(const std::function<void(CrashUploadState state)> &progressCallback);

protected:
    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
};
