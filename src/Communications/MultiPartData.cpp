#include "MultiPartData.hpp"

#include <StringUtil/StringUtil.hpp>
#include <cstring>

std::string MultiPartData::boundary = "SIND12345";

auto MultiPartData::addKeyValue(const std::string &key, const std::string &value) -> bool {
    return addKeyValue(key, reinterpret_cast<const uint8_t *>(value.c_str()), value.length(),
                       false);
}

auto MultiPartData::startKeyWrite() -> bool {
    if (!keyWritten_) {
        keyWritten_ = true;
        tempFile_ = fileSystem_.open(fileName_, FILE_WRITE, true);
        // write first boundary marker if this is the first key
        if (!SafeSnprintf(tmp_, sizeof(tmp_), "--%s--", boundary.c_str())) {
            return false;
        }

        if (tempFile_.write(reinterpret_cast<uint8_t *>(tmp_), strlen(tmp_)) != strlen(tmp_)) {
            return false;
        }
    }

    return true;
}

auto MultiPartData::writeHeader(const std::string &key, bool isFile) -> bool {
    // write form header
    if (!SafeSnprintf(tmp_, sizeof(tmp_), "\r\nContent-Disposition: form-data; name=\"%s\"",
                      key.c_str())) {
        return false;
    }
    if (tempFile_.write(reinterpret_cast<uint8_t *>(tmp_), strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }
    if (isFile) {
        if (!SafeSnprintf(tmp_, sizeof(tmp_), "; filename=\"%s\"\r\n", fileName_.c_str())) {
            return false;
        }
    } else {
        if (!SafeSnprintf(tmp_, sizeof(tmp_), "\r\n\r\n")) {
            return false;
        }
    }

    if (tempFile_.write(reinterpret_cast<uint8_t *>(tmp_), strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }

    return true;
}

auto MultiPartData::writeFooter() -> bool {
    if (!SafeSnprintf(tmp_, sizeof(tmp_), "\r\n--%s", boundary.c_str())) {
        return false;
    }
    if (tempFile_.write(reinterpret_cast<uint8_t *>(tmp_), strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }

    return true;
}

auto MultiPartData::addKeyValue(const std::string &key, const uint8_t *value, int len, bool isFile)
    -> bool {
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

auto MultiPartData::startFile(const std::string &key) -> bool {
    if (!startKeyWrite()) {
        return false;
    }

    if (!writeHeader(key, true)) {
        return false;
    }

    if (!SafeSnprintf(tmp_, sizeof(tmp_), "Content-Type: application/octet-stream\r\n\r\n")) {
        return false;
    }
    if (tempFile_.write(reinterpret_cast<uint8_t *>(tmp_), strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }

    return true;
}

auto MultiPartData::writeFileChunk(uint8_t *chunk, int len) -> bool {
    log_i("trying to write %d bytes", len);
    if (tempFile_.write(chunk, len) != len) {
        log_i("write failed!");
        return false;
    }

    log_i("success %d bytes", len);
    return true;
}

auto MultiPartData::endFile() -> bool {
    if (!writeFooter()) {
        return false;
    }

    return true;
}

auto MultiPartData::getContentType() -> std::string {
    char tmp[64];
    SafeSnprintf(tmp, sizeof(tmp), "multipart/form-data; boundary=%s", boundary.c_str());
    return tmp;
}

auto MultiPartData::endMultiPartData() -> bool {
    log_i("end multipart data!");

    if (!SafeSnprintf(tmp_, sizeof(tmp_), "--\r\n")) {
        return false;
    }
    if (tempFile_.write(reinterpret_cast<uint8_t *>(tmp_), strlen(tmp_)) != strlen(tmp_)) {
        return false;
    }
    tempFile_.close();

    return true;
}

auto MultiPartData::getFile() -> SFile {
    tempFile_ = fileSystem_.open(fileName_, FILE_READ);
    return tempFile_;
}
