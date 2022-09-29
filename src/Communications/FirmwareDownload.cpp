#include "FirmwareDownload.hpp"

#include <cstdio>
#include <string>

void FirmwareDownload::getFirmware() {

    log_i("about to download: %s", FIRMWARE_URL);
    if (!httpClient_.begin(FIRMWARE_URL)) {
        log_e("HTTPClient::begin failed");
    }

    httpClient_.setTimeout(30000);
    httpClient_.GET();

    _totalSize = httpClient_.getSize();
    log_i("request started, total size %d", _totalSize);
}

auto FirmwareDownload::updateFirmware(int &percentProgress) -> bool {

    // check available bytes
    if (_totalBytesRead == _totalSize) {
        // TODO: write last block to alternate boot partition
        log_i("done. read %d", _totalBytesRead);

        // mark alternate boot partition as the primary and reboot
        return true;
    }

    auto *stream = httpClient_.getStreamPtr();

    if (stream) {
        int available = stream->available();
        if (available > 0) {
            int bytesRemaining = FIRMWARE_BLOCK_SIZE - _bytesInBuffer;
            int bytesRead = stream->readBytes(&buffer_[_bytesInBuffer], bytesRemaining);
            _bytesInBuffer += bytesRead;
            _totalBytesRead += bytesRead;
        }

        if (_bytesInBuffer == FIRMWARE_BLOCK_SIZE) {
            // have one full block of firmware bytes, write to alternate boot partition
            log_i("read full block! %d", _totalBytesRead);
            _bytesInBuffer = 0;
        }
    }

    percentProgress = (int)((float)_totalBytesRead * 100.0 / (float)_totalSize);
    return false;
}