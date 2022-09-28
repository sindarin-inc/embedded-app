#include "FirmwareDownload.hpp"
#include <string>
#include <stdio.h>

void FirmwareDownload::GetFirmware() {

    log_i("about to download: %s", FIRMWARE_URL);
    if (!_httpClient.begin(FIRMWARE_URL)) {
        log_e("HTTPClient::begin failed");
    }

    _httpClient.setTimeout(30000);
    _httpClient.GET();

    _totalSize = _httpClient.getSize();
    log_i("request started, total size %d", _totalSize);
}

bool FirmwareDownload::UpdateFirmware(int& percentProgress) {
    
    // check available bytes
    if (_totalBytesRead == _totalSize)
    {
        // TODO: write last block to alternate boot partition
        log_i("done. read %d", _totalBytesRead);

        // mark alternate boot partition as the primary and reboot
        return true;
    }

    WiFiClient* stream = _httpClient.getStreamPtr();
    if (stream)
    {
        int available = stream->available();
        if (available > 0)
        {
            int bytesRemaining = FIRMWARE_BLOCK_SIZE - _bytesInBuffer;
            int bytesRead = stream->readBytes(&_buffer[_bytesInBuffer], bytesRemaining);
            _bytesInBuffer += bytesRead;
            _totalBytesRead += bytesRead;
        }

        if (_bytesInBuffer == FIRMWARE_BLOCK_SIZE)
        {
            // have one full block of firmware bytes, write to alternate boot partition
            log_i("read full block! %d", _totalBytesRead);
            _bytesInBuffer = 0;
        }
    }

    percentProgress = (int)((float)_totalBytesRead  * 100.0 / (float)_totalSize);
    return false;
}