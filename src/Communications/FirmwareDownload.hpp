#pragma once
#ifndef FirmwareDownload_H
#define FirmwareDownload_H
#include <HTTPClient.h>
#include <cstdint>

#define FIRMWARE_BLOCK_SIZE 16384
#define FIRMWARE_URL "http://change.this.url/firmware.bin"
#define FIRMWARE_MD5_URL "http://change.this.url/firmware.md5"
#define FIRMWARE_CHECK_MD5 1
#define FIRMWARE_MD5_SIZE 32

class FirmwareDownload {
public:
    FirmwareDownload() = default;

    auto getFirmware() -> bool;
    auto updateFirmware(int &percentProgress, bool &error) -> bool;

private:
    auto writeBuffer() -> bool;

    HTTPClient httpClient_;
    uint8_t *buffer_{nullptr};
    int bytesInBuffer_{0};
    int totalSize_{0};
    int totalBytesRead_{0};
    char md5_[33];
};

#endif