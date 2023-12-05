#include "SFile.hpp"

#include <cstring>

#include "sindarin-debug.h"

// SFile
auto SFile::write(uint8_t c) -> size_t { return write(&c, 1); }

auto SFile::write(const uint8_t *buf, size_t size) -> size_t {
    if (!*this) {
        return 0;
    }
    return p_->write(buf, size);
}
auto SFile::available() -> int {
    if (!*this) {
        return false;
    }

    return p_->size() - p_->position();
}
auto SFile::read() -> int {
    if (!*this) {
        return -1;
    }

    uint8_t result;
    if (p_->read(&result, 1) != 1) {
        return -1;
    }

    return result;
}
auto SFile::peek() -> int {
    if (!*this) {
        return -1;
    }

    size_t curPos = p_->position();
    int result = read();
    seek(curPos, SSeekSet);
    return result;
}
auto SFile::flush() -> int {
    if (!*this) {
        return 0;
    }

    return p_->flush();
}
auto SFile::read(uint8_t *buf, size_t size) -> size_t {
    if (!*this) {
        return -1;
    }

    return p_->read(buf, size);
}
auto SFile::readBytes(char *buffer, size_t length) -> size_t {
    return read(reinterpret_cast<uint8_t *>(buffer), length);
}

auto SFile::seek(uint32_t pos, SSeekMode mode) -> bool {
    if (!*this) {
        return false;
    }

    return p_->seek(pos, mode);
}

auto SFile::position() const -> size_t {
    if (!*this) {
        return 0;
    }

    return p_->position();
}
auto SFile::size() const -> size_t {
    if (!*this) {
        return 0;
    }

    return p_->size();
}
auto SFile::close() -> int {
    int retval = 0;
    if (p_) {
        retval = p_->close();
        p_ = nullptr;
    }
    return retval;
}

SFile::operator bool() const { return p_ != nullptr && *p_ != false; }

// VFSFile
auto VFSFile::write(const uint8_t *buf, size_t size) -> size_t {
    if (!*this) {
        return 0;
    }

    return fwrite(buf, 1, size, fp_);
}

auto VFSFile::available() const -> int {
    if (!*this) {
        return false;
    }
    return size() - position();
}

auto VFSFile::flush() -> int {
    if (!*this) {
        return 0;
    }

    return fflush(fp_);
}

auto VFSFile::read(uint8_t *buf, size_t size) -> size_t {
    if (!*this) {
        return -1;
    }

    return fread(buf, 1, size, fp_);
}

auto VFSFile::seek(uint32_t pos, SSeekMode mode) -> bool {
    if (!*this) {
        return false;
    }

    if (mode == SSeekSet) {
        return fseek(fp_, pos, SEEK_SET) == 0;
    } else if (mode == SSeekCur) {
        return fseek(fp_, pos, SEEK_CUR) == 0;
    } else if (mode == SSeekEnd) {
        return fseek(fp_, pos, SEEK_END) == 0;
    }

    return false;
}

auto VFSFile::position() const -> size_t {
    if (!*this) {
        return 0;
    }

    return ftell(fp_);
}

void VFSFile::getFileSize() {
    size_t curPos = position();
    seek(static_cast<uint32_t>(0), SSeekEnd);
    fSize_ = position();
    seek(curPos, SSeekSet);
}

auto VFSFile::size() const -> size_t {
    if (!*this) {
        return 0;
    }
    return fSize_;
}
auto VFSFile::close() -> int {
    int retval = 0;
    if (fp_) {
        retval = fclose(fp_);
        fp_ = nullptr;
    }
    return retval;
}

VFSFile::operator bool() const {
    if (fp_) {
        return true;
    }

    return false;
}

auto VFSFile::getFilePtr() -> FILE * { return fp_; }