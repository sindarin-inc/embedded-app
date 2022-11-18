#include "MultiPartData.hpp"

#include <cstring>

void MultiPartData::addKeyValue(const std::string &key, const std::string &value) {
    addKeyValue(key, (uint8_t *)value.c_str(), value.length(), false);
}

void MultiPartData::startKeyWrite() {
    if (!keyWritten_) {
        tempFile_ = fileSystem_.open(fileName_, FILE_WRITE, true);
        keyWritten_ = true;

        // write first boundary marker if this is the first key
        snprintf(tmp_, sizeof(tmp_), "--%s--", boundary_.c_str());
        tempFile_.write((uint8_t *)tmp_, strlen(tmp_));
    }
}

void MultiPartData::writeHeader(const std::string &key, bool isFile) {
    // write form header
    snprintf(tmp_, sizeof(tmp_), "\r\nContent-Disposition: form-data; name=\"%s\"", key.c_str());
    tempFile_.write((uint8_t *)tmp_, strlen(tmp_));
    if (isFile) {
        // TODO: better filename
        snprintf(tmp_, sizeof(tmp_), "; filename=\"temp\"\r\n");
    } else {
        snprintf(tmp_, sizeof(tmp_), "\r\n\r\n");
    }

    tempFile_.write((uint8_t *)tmp_, strlen(tmp_));
}

void MultiPartData::writeFooter() {
    snprintf(tmp_, sizeof(tmp_), "\r\n--%s", boundary_.c_str());
    tempFile_.write((uint8_t *)tmp_, strlen(tmp_));
}

void MultiPartData::addKeyValue(const std::string &key, uint8_t *value, int len, bool isFile) {
    startKeyWrite();
    writeHeader(key, isFile);
    // now write the actual data
    tempFile_.write(value, len);
    writeFooter();
}

void MultiPartData::startFile(const std::string &key) {
    startKeyWrite();
    writeHeader(key, true);

    snprintf(tmp_, sizeof(tmp_), "Content-Type: application/octet-stream\r\n\r\n");
    tempFile_.write((uint8_t *)tmp_, strlen(tmp_));
}

void MultiPartData::writeFileChunk(uint8_t *chunk, int len) { tempFile_.write(chunk, len); }

void MultiPartData::endFile() { writeFooter(); }

auto MultiPartData::getContentType() -> String {
    snprintf(tmp_, sizeof(tmp_), "multipart/form-data; boundary=%s", boundary_.c_str());
    return tmp_;
}

void MultiPartData::endMultiPartData() {
    snprintf(tmp_, sizeof(tmp_), "--\r\n");
    tempFile_.write((uint8_t *)tmp_, strlen(tmp_));
    tempFile_.close();
}

auto MultiPartData::getFile() -> File {
    tempFile_ = fileSystem_.open(fileName_, FILE_READ);
    return tempFile_;
}