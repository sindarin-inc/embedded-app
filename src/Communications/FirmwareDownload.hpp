#pragma once
#ifndef FirmwareDownload_H
#define FirmwareDownload_H
#include <HTTPClient.h>
#include <stdint.h>

#define FIRMWARE_BLOCK_SIZE 1024
#define FIRMWARE_URL "http://replace.this.url/firmware.bin"

class FirmwareDownload {
public:
    FirmwareDownload()   
    : _bytesInBuffer(0)
    , _totalSize(0)
    , _totalBytesRead(0) {}

    void GetFirmware();
    bool UpdateFirmware(int& percentProgress);

private:
    HTTPClient _httpClient;
    uint8_t _buffer[FIRMWARE_BLOCK_SIZE];
    int _bytesInBuffer;
    int _totalSize;
    int _totalBytesRead;
};

#endif