#pragma once
#ifndef FirmwareDownload_H
#define FirmwareDownload_H
#include <HTTPClient.h>
#include <cstdint>

#define FIRMWARE_BLOCK_SIZE 16384
#define FIRMWARE_URL "http://change.this.url/firmware.bin"

class FirmwareDownload {
public:
    FirmwareDownload() = default;

    void getFirmware();
    auto updateFirmware(int &percentProgress, bool &error) -> bool;

private:
    auto writeBuffer() -> bool;

    HTTPClient httpClient_;
    uint8_t *buffer_{nullptr};
    int bytesInBuffer_{0};
    int totalSize_{0};
    int totalBytesRead_{0};
};

#endif