#pragma once
#ifndef FirmwareDownload_H
#define FirmwareDownload_H
#include <HTTPClient.h>
#include <cstdint>
#include <utility>

#define FIRMWARE_BLOCK_SIZE 4096
#define FIRMWARE_CHECK_MD5 1
#define FIRMWARE_MD5_SIZE 32

class FirmwareDownload {
public:
    FirmwareDownload(std::string url, std::string md5)
        : url_(std::move(url)), md5_(std::move(md5)){};

    auto getFirmware() -> bool;
    auto updateFirmware(int &percentProgress, bool &error) -> bool;

private:
    auto writeBuffer() -> bool;

    std::string url_;
    std::string md5_;

    HTTPClient httpClient_;
    uint8_t *buffer_{nullptr};
    int bytesInBuffer_{0};
    int totalSize_{0};
    int totalBytesRead_{0};
};

#endif