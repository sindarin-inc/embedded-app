#pragma once

#include <HTTPClient.h>
#include <cstdint>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Storage/PreferencesStore.hpp"

#define FIRMWARE_BLOCK_SIZE 4096
#define FIRMWARE_CHECK_MD5 1
#define FIRMWARE_MD5_SIZE 32

struct FirmwareDownloadState {
    enum State {
        Idle,
        ConnectingWifi,
        CheckingForLatest,
        Downloading,
        Installing,
        Installed,
        Failure,
        AlreadyUpToDate,
    } state = Idle;

    int downloadPercentage;
    std::string message;
};

class FirmwareDownloader {
public:
    FirmwareDownloader(PreferencesStore &prefs) : prefs_(prefs){};

    void downloadFirmware(const std::function<void(FirmwareDownloadState state)> &progressCallback);

    std::string versionString;

protected:
    PreferencesStore &prefs_;
};
