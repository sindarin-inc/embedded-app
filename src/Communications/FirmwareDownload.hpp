#pragma once
#ifndef FirmwareDownload_H
#define FirmwareDownload_H
#include <HTTPClient.h>
#include <Update.h>
#include <cstdint>

#define FIRMWARE_BLOCK_SIZE 16384
#define FIRMWARE_URL "http://bitfixer.com/tmp/firmware.bin"

class FirmwareDownload {
public:
    FirmwareDownload() : buffer_(nullptr) {}

    void getFirmware();
    auto updateFirmware(int &percentProgress, bool &error) -> bool;

private:
    auto writeBuffer() -> bool;

    HTTPClient httpClient_;
    uint8_t *buffer_;
    int _bytesInBuffer{0};
    int _totalSize{0};
    int _totalBytesRead{0};
};

#endif