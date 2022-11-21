#include "MultiPartData.hpp"

#include <cstring>

std::string MultiPartData::boundary_ = "SIND12345";

bool MultiPartData::addKeyValue(const std::string &key, const std::string &value) {
    return addKeyValue(key, (uint8_t *)value.c_str(), value.length(), false);
}

bool MultiPartData::startKeyWrite() {
    if (!keyWritten_) {
        keyWritten_ = true;
        tempFile_ = fileSystem_.open(fileName_, FILE_WRITE, true);
        // write first boundary marker if this is the first key
        snprintf(tmp_, sizeof(tmp_), "--%s--", boundary_.c_str());
        if (tempFile_.write((uint8_t *)tmp_, strlen(tmp_)) != strlen(tmp_)) {
            return false;
        }
    }

    return true;
}

bool MultiPartData::writeHeader(const std::string &key, bool isFile) {
    // write form header
    snprintf(tmp_, sizeof(tmp_), "\r\nContent-Disposition: form-data; name=\"%s\"", key.c_str());
    if (tempFile_.write((uint8_t *)tmp_, strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }
    if (isFile) {
        snprintf(tmp_, sizeof(tmp_), "; filename=\"%s\"\r\n", fileName_.c_str());
    } else {
        snprintf(tmp_, sizeof(tmp_), "\r\n\r\n");
    }

    if (tempFile_.write((uint8_t *)tmp_, strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }

    return true;
}

bool MultiPartData::writeFooter() {
    snprintf(tmp_, sizeof(tmp_), "\r\n--%s", boundary_.c_str());
    if (tempFile_.write((uint8_t *)tmp_, strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }

    return true;
}

bool MultiPartData::addKeyValue(const std::string &key, uint8_t *value, int len, bool isFile) {
    if (!startKeyWrite()) {
        return false;
    }

    if (!writeHeader(key, isFile)) {
        return false;
    }
    // now write the actual data
    if (tempFile_.write(value, len) != len) {
        return false;
    }

    if (!writeFooter()) {
        return false;
    }

    return true;
}

bool MultiPartData::startFile(const std::string &key) {
    if (!startKeyWrite()) {
        return false;
    }

    if (!writeHeader(key, true)) {
        return false;
    }

    snprintf(tmp_, sizeof(tmp_), "Content-Type: application/octet-stream\r\n\r\n");
    if (tempFile_.write((uint8_t *)tmp_, strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }

    return true;
}

bool MultiPartData::writeFileChunk(uint8_t *chunk, int len) {
    log_i("trying to write %d bytes", len);
    if (tempFile_.write(chunk, len) != len) {
        log_i("write failed!");
        return false;
    }

    log_i("success %d bytes", len);
    return true;
}

bool MultiPartData::endFile() {
    if (!writeFooter()) {
        return false;
    }

    return true;
}

auto MultiPartData::getContentType() -> String {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "multipart/form-data; boundary=%s", boundary_.c_str());
    return tmp;
}

bool MultiPartData::endMultiPartData() {
    log_i("end multipart data!");

    snprintf(tmp_, sizeof(tmp_), "--\r\n");
    if (tempFile_.write((uint8_t *)tmp_, strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }
    tempFile_.close();

    return true;
}

auto MultiPartData::getFile() -> File {
    tempFile_ = fileSystem_.open(fileName_, FILE_READ);
    return tempFile_;
}