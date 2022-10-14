#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>

auto FirmwareDownload::getFirmware() -> bool {
    if (!httpClient_.begin(String(url_.c_str()))) {
        log_e("HTTPClient::begin failed");
        return false;
    }

    httpClient_.setTimeout(30000);
    if (httpClient_.GET() != 200) {
        log_e("failed to download %s", url_.c_str());
        return false;
    }

    totalSize_ = httpClient_.getSize();
    log_i("request started, total size %d", totalSize_);

    if (totalSize_ > 0) {
        Update.begin(totalSize_, U_FLASH);
        log_i("setting expected md5: %s", md5_.c_str());
        if (!Update.setMD5(md5_.c_str())) {
            log_e("error setting md5");
            return false;
        }
    }

    return true;
}

auto FirmwareDownload::writeBuffer() -> bool {
    int written = Update.write(buffer_, bytesInBuffer_);
    bool success = true;
    if (written != bytesInBuffer_) {
        log_e("FirmwareDownload::writeBuffer - expected %d wrote %d", bytesInBuffer_, written);
        success = false;
    }

    bytesInBuffer_ = 0;
    return success;
}

auto FirmwareDownload::updateFirmware(int &percentProgress, bool &error) -> bool {

    // check available bytes
    if (totalBytesRead_ == totalSize_) {
        httpClient_.end();
        // write last block to alternate boot partition
        if (bytesInBuffer_ > 0) {
            error = !writeBuffer();
        }

        delete buffer_;
        buffer_ = nullptr;

        log_i("Firmware download done. read %d bytes. Finalizing...", totalBytesRead_);
        if (!error && Update.end()) {
            // mark alternate boot partition as the primary and reboot
            log_i("Update complete. Boot partition swapped.");
        } else {
            log_e("Update failed to finalize.");
            error = true;
        }

        return true;
    }

    auto *stream = httpClient_.getStreamPtr();

    if (stream) {
        int available = stream->available();
        if (available > 0) {
            if (buffer_ == nullptr) {
                buffer_ = new uint8_t[FIRMWARE_BLOCK_SIZE];
            }

            int bytesRemainingInBlock = FIRMWARE_BLOCK_SIZE - bytesInBuffer_;
            int bytesToRead = bytesRemainingInBlock;
            if (available < bytesToRead) {
                bytesToRead = available;
            }
            int bytesRead = stream->readBytes(&buffer_[bytesInBuffer_], bytesToRead);
            bytesInBuffer_ += bytesRead;
            totalBytesRead_ += bytesRead;
        }

        if (bytesInBuffer_ == FIRMWARE_BLOCK_SIZE) {
            // have one full block of firmware bytes, write to alternate boot partition
            error = !writeBuffer();
        }
    } else {
        // error getting stream pointer, firmware update failed (bad URL, bad connection)
        log_e("Firmware Update: failed to open connection to %s", url_.c_str());
        error = true;
    }

    percentProgress = (int)((float)totalBytesRead_ * 100.0 / (float)totalSize_);
    return false;
}