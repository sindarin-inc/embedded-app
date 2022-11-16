#pragma once

#include <HTTPClient.h>
#include <cstdint>
#include <utility>
#include "Storage/FileSystem.hpp"

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
    CrashUploader(FileSystem &fileSystem) 
    : fileSystem_(fileSystem)
    {};

    void uploadCrash(const std::function<void(CrashUploadState state)> &progressCallback);

protected:
    FileSystem &fileSystem_;
};
