#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>

auto FirmwareDownload::getFirmware() -> bool {

#ifdef FIRMWARE_CHECK_MD5
    // first download md5 file
    if (!httpClient_.begin(FIRMWARE_MD5_URL)) {
        log_e("HTTPClient::begin failed for %s", FIRMWARE_MD5_URL);
        return false;
    }

    httpClient_.setTimeout(30000);
    if (httpClient_.GET() != 200) {
        log_e("could not download %s", FIRMWARE_MD5_URL);
        return false;
    }

    int md5Size = httpClient_.getSize();
    if (md5Size < FIRMWARE_MD5_SIZE || md5Size > FIRMWARE_MD5_SIZE + 1) {
        log_e("bad md5 size, %d should be 32", md5Size);
        return false;
    }

    String md5string = httpClient_.getString();
    strncpy(md5_, md5string.c_str(), FIRMWARE_MD5_SIZE);
    md5_[FIRMWARE_MD5_SIZE] = 0;
    httpClient_.end();
#endif

    if (!httpClient_.begin(FIRMWARE_URL)) {
        log_e("HTTPClient::begin failed");
        return false;
    }

    httpClient_.setTimeout(30000);
    if (httpClient_.GET() != 200) {
        log_e("failed to download %s", FIRMWARE_URL);
        return false;
    }

    totalSize_ = httpClient_.getSize();
    log_i("request started, total size %d", totalSize_);

    if (totalSize_ > 0) {
        Update.begin(totalSize_, U_FLASH);
#ifdef FIRMWARE_CHECK_MD5
        log_i("setting expected md5: %s", md5_);
        if (!Update.setMD5(md5_)) {
            log_e("error setting md5");
            return false;
        }
#endif
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
            log_i("update complete.");
            log_i("swapping boot partition");
            ESP.restart();
        } else {
            log_e("update failed to finalize.");
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
        log_e("Firmware Update: failed to open connection to %s", FIRMWARE_URL);
        error = true;
    }

    percentProgress = (int)((float)totalBytesRead_ * 100.0 / (float)totalSize_);
    return false;
}