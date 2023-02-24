#include "SFile.hpp"

#include "sindarin-debug.h"

// SFile
size_t SFile::write(uint8_t c) { return write(&c, 1); }

size_t SFile::write(const uint8_t *buf, size_t size) {
    if (!*this) {
        return 0;
    }
    return _p->write(buf, size);
}
int SFile::available() {
    if (!*this) {
        return false;
    }

    return _p->size() - _p->position();
}
int SFile::read() {
    if (!*this) {
        return -1;
    }

    uint8_t result;
    if (_p->read(&result, 1) != 1) {
        return -1;
    }

    return result;
}
int SFile::peek() {
    if (!*this) {
        return -1;
    }

    size_t curPos = _p->position();
    int result = read();
    seek(curPos, SSeekSet);
    return result;
}
void SFile::flush() {
    if (!*this) {
        return;
    }

    _p->flush();
}
size_t SFile::read(uint8_t *buf, size_t size) {
    if (!*this) {
        return -1;
    }

    return _p->read(buf, size);
}
size_t SFile::readBytes(char *buffer, size_t length) { return read((uint8_t *)buffer, length); }

bool SFile::seek(uint32_t pos, SSeekMode mode) {
    if (!*this) {
        return false;
    }

    return _p->seek(pos, mode);
}

size_t SFile::position() const {
    if (!*this) {
        return 0;
    }

    return _p->position();
}
size_t SFile::size() const {
    if (!*this) {
        return 0;
    }

    return _p->size();
}
void SFile::close() {
    if (_p) {
        _p->close();
        _p = nullptr;
    }
}

SFile::operator bool() const { return _p != nullptr && *_p != false; }

// VFSFile
size_t VFSFile::write(const uint8_t *buf, size_t size) {
    if (!*this) {
        return 0;
    }

    return fwrite(buf, 1, size, fp_);
}

int VFSFile::available() {
    if (!*this) {
        return false;
    }
    return size() - position();
}

void VFSFile::flush() {
    if (!*this) {
        return;
    }

    fflush(fp_);
}

size_t VFSFile::read(uint8_t *buf, size_t size) {
    if (!*this) {
        return -1;
    }

    return fread(buf, 1, size, fp_);
}

bool VFSFile::seek(uint32_t pos, SSeekMode mode) {
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

size_t VFSFile::position() const {
    if (!*this) {
        return 0;
    }

    return ftell(fp_);
}

void VFSFile::getFileSize() {
    size_t curPos = position();
    seek((uint32_t)0, SSeekEnd);
    fSize_ = position();
    seek(curPos, SSeekSet);
}

size_t VFSFile::size() const {
    if (!*this) {
        return 0;
    }
    return fSize_;
}
void VFSFile::close() {
    if (fp_) {
        fclose(fp_);
        fp_ = nullptr;
    }
}

VFSFile::operator bool() const {
    if (fp_) {
        return true;
    }

    return false;
}