#pragma once

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
        AlreadyDownloaded,
        Downloaded,
        Installing,
        Installed,
        Failure,
        AlreadyUpToDate,
    } state = Idle;

    int percentage{};
    std::string message;

    auto operator==(const FirmwareDownloadState &other) const -> bool {
        return state == other.state && percentage == other.percentage;
    }
};

class FirmwareDownloader {
public:
    FirmwareDownloader(PreferencesStore &prefs, bool skipInstall = false)
        : prefs_(prefs), skipInstall_(skipInstall){};

    void downloadFirmware(const std::function<void(FirmwareDownloadState state)> &progressCallback);
    void cancel() { cancelled_ = true; }

    std::string versionString;
    FirmwareResponse response;

protected:
    PreferencesStore &prefs_;
    bool skipInstall_;
    bool cancelled_ = false;
};
