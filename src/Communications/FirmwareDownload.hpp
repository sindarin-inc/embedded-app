#pragma once
#ifndef FirmwareDownload_H
#define FirmwareDownload_H
#include <HTTPClient.h>
#include <stdint.h>
#include <Update.h>

#define FIRMWARE_BLOCK_SIZE 16384
#define FIRMWARE_URL "http://bitfixer.com/tmp/firmware.bin"

class FirmwareDownload {
public:
    FirmwareDownload()
    : _buffer(NULL)   
    , _bytesInBuffer(0)
    , _totalSize(0)
    , _totalBytesRead(0) {}

    void GetFirmware();
    bool UpdateFirmware(int& percentProgress, bool& error);

private:

    bool writeBuffer();

    HTTPClient _httpClient;
    uint8_t* _buffer;
    int _bytesInBuffer;
    int _totalSize;
    int _totalBytesRead;
};

#endif