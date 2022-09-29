#include "FirmwareDownload.hpp"

#include <cstdio>
#include <string>
#include <Update.h>

void FirmwareDownload::getFirmware() {

    log_i("about to download: %s", FIRMWARE_URL);
    if (!httpClient_.begin(FIRMWARE_URL)) {
        log_e("HTTPClient::begin failed");
    }

    httpClient_.setTimeout(30000);
    httpClient_.GET();

    _totalSize = httpClient_.getSize();
    log_i("request started, total size %d", _totalSize);

    if (_totalSize > 0) {
        Update.begin(_totalSize, U_FLASH);
    }
}

auto FirmwareDownload::writeBuffer() -> bool {
    int written = Update.write(buffer_, _bytesInBuffer);
    bool success = true;
    if (written != _bytesInBuffer) {
        log_e("FirmwareDownload::writeBuffer - expected %d wrote %d", _bytesInBuffer, written);
        success = false;
    }

    _bytesInBuffer = 0;
    return success;
}

auto FirmwareDownload::updateFirmware(int &percentProgress, bool &error) -> bool {

    // check available bytes
    if (_totalBytesRead == _totalSize) {
        httpClient_.end();
        // write last block to alternate boot partition
        if (_bytesInBuffer > 0) {
            error = !writeBuffer();
        }

        delete buffer_;
        buffer_ = nullptr;

        log_i("Firmware download done. read %d bytes. Finalizing...", _totalBytesRead);
        if (!error && Update.end()) {
            log_i("update complete.");
            log_i("swapping boot partition");
            ESP.restart();
        } else {
            log_e("update failed to finalize.");
        }

        // mark alternate boot partition as the primary and reboot
        return true;
    }

    auto *stream = httpClient_.getStreamPtr();

    if (stream) {
        int available = stream->available();
        if (available > 0) {
            if (buffer_ == nullptr) {
                buffer_ = new uint8_t[FIRMWARE_BLOCK_SIZE];
            }

            int bytesRemainingInBlock = FIRMWARE_BLOCK_SIZE - _bytesInBuffer;
            int bytesToRead = bytesRemainingInBlock;
            if (available < bytesToRead) {
                bytesToRead = available;
            }
            int bytesRead = stream->readBytes(&buffer_[_bytesInBuffer], bytesToRead);
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