#include "FirmwareDownload.hpp"

#include <cstdio>
#include <string>

void FirmwareDownload::GetFirmware() {

    log_i("about to download: %s", FIRMWARE_URL);
    if (!_httpClient.begin(FIRMWARE_URL)) {
        log_e("HTTPClient::begin failed");
    }

    _httpClient.setTimeout(30000);
    _httpClient.GET();

    _totalSize = _httpClient.getSize();
    log_i("request started, total size %d", _totalSize);

    if (_totalSize > 0)
    {
        Update.begin(_totalSize, U_FLASH);
    }
}

bool FirmwareDownload::writeBuffer() {
    int written = Update.write(_buffer, _bytesInBuffer);
    bool success = true;
    if (written != _bytesInBuffer)
    {
        log_e("FirmwareDownload::writeBuffer - expected %d wrote %d", _bytesInBuffer, written);
        success = false;
    }

    _bytesInBuffer = 0;
    return success;
}

bool FirmwareDownload::UpdateFirmware(int& percentProgress, bool& error) {
    
    // check available bytes
    if (_totalBytesRead == _totalSize)
    {
        _httpClient.end();
        // write last block to alternate boot partition
        if (_bytesInBuffer > 0)
        {
            error = !writeBuffer();
        }

        delete _buffer;
        _buffer = NULL;

        log_i("Firmware download done. read %d bytes. Finalizing...", _totalBytesRead);
        if (!error && Update.end())
        {
            log_i("update complete.");
            log_i("swapping boot partition");
            ESP.restart();
        }
        else
        {
            log_e("update failed to finalize.");
        }

        // mark alternate boot partition as the primary and reboot
        return true;
    }

    auto *stream = _httpClient.getStreamPtr();

    if (stream) {
        int available = stream->available();
        if (available > 0)
        {
            if (_buffer == NULL)
            {
                _buffer = new uint8_t[FIRMWARE_BLOCK_SIZE];
            }

            int bytesRemainingInBlock = FIRMWARE_BLOCK_SIZE - _bytesInBuffer;
            int bytesToRead = bytesRemainingInBlock;
            if (available < bytesToRead)
            {
                bytesToRead = available;
            }
            int bytesRead = stream->readBytes(&_buffer[_bytesInBuffer], bytesToRead);
            _bytesInBuffer += bytesRead;
            _totalBytesRead += bytesRead;
        }

        if (_bytesInBuffer == FIRMWARE_BLOCK_SIZE) {
            // have one full block of firmware bytes, write to alternate boot partition
            error = !writeBuffer();
        }
    }

    percentProgress = (int)((float)_totalBytesRead * 100.0 / (float)_totalSize);
    return false;
}