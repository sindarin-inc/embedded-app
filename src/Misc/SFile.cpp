#include "SFile.hpp"
#include "sindarin-debug.h"

// SFile

size_t SFile::write(uint8_t c) {
    
}
size_t SFile::write(const uint8_t *buf, size_t size) {
    return _p->write(buf, size);
}
int SFile::available() {
    return _p->available();
}
int SFile::read() {

}
int SFile::peek() {

}
void SFile::flush() {

}
size_t SFile::read(uint8_t *buf, size_t size) {

}
size_t SFile::readBytes(char *buffer, size_t length) {
    
}

bool SFile::seek(uint32_t pos, SSeekMode mode) {

}
bool SFile::seek(uint32_t pos) { return seek(pos, SSeekSet); }
size_t SFile::position() const {

}
size_t SFile::size() const {

}
void SFile::close() {

}
//time_t getLastWrite() {}
const char *SFile::name() const {

}
const char *SFile::path() const {

}

boolean SFile::isDirectory(void) {

}
//File openNextFile(const char *mode = FILE_READ) {}
void SFile::rewindDirectory(void) {

}


// VFSFile
size_t VFSFile::write(uint8_t) {
    return 0;
}

size_t VFSFile::write(const uint8_t *buf, size_t size) {
    return 0;
}

int VFSFile::available() {
    return size() - position();
}

int VFSFile::read() {
    uint8_t c;
    size_t r = read(&c, 1);
    if (r == -1) {
        return -1;
    }

    return c;
}

int VFSFile::peek() {
    if (!fp_) {
        return -1;
    }

    size_t curPos = position();
    int result = read();
    seek(curPos, SSeekSet);
    return result;
}

void VFSFile::flush() {
    if (!fp_) {
        return;
    }

    fflush(fp_);
}

size_t VFSFile::read(uint8_t *buf, size_t size) {
    log_i("SFile read %d", size);
    if (!fp_) {
        log_i("no file pointer");
        return -1;
    }

    size_t ff = fread(buf, 1, size, fp_);
    log_i("Sfile fread %d", ff);
    return ff;
}
size_t VFSFile::readBytes(char *buffer, size_t length) {
    return read((uint8_t *)buffer, length);
}

bool VFSFile::seek(uint32_t pos, SSeekMode mode) {
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
    if (fp_) {
        return ftell(fp_);
    }

    // error
    return -1;
}

void VFSFile::getFileSize() {
    size_t curPos = position();
    seek((uint32_t)0, SSeekEnd);
    fSize_ = position();
    seek(curPos, SSeekSet);
}

size_t VFSFile::size() const {
    return fSize_;
}
void VFSFile::close() {
    fclose(fp_);
    fp_ = NULL;
}
//time_t getLastWrite(){}
const char *VFSFile::name() const {
    return nullptr;
}
const char *VFSFile::path() const {
    return nullptr;
}

bool VFSFile::isDirectory(void) {
    return false;
}

/*
SFile VFSFile::openNextFile(const char *mode = "r") {

}
*/

void VFSFile::rewindDirectory(void){}