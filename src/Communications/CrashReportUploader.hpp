#pragma once

#include <HTTPClient.h>
#include <cstdint>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Storage/PreferencesStore.hpp"

#define FIRMWARE_BLOCK_SIZE 4096
#define FIRMWARE_CHECK_MD5 1
#define FIRMWARE_MD5_SIZE 32

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
    CrashUploader(PreferencesStore &prefs, FileSystem &fileSystem) 
    : prefs_(prefs)
    , fileSystem_(fileSystem)
    {};

    void uploadCrash(const std::function<void(CrashUploadState state)> &progressCallback);

    std::string versionString;

protected:
    PreferencesStore &prefs_;
    FileSystem &fileSystem_;
};
