#pragma once

#include "FS.h"
#include "Misc/GUnzip.hpp"

class GUnzipFile {
public:
    GUnzipFile(File compressedFile) 
    : compressedFile_(compressedFile)
    , position_(0) 
    , inSize_(0)
    , inIndex_(0)
    , outSize_(0)
    , outIndex_(0)
    , lastInput_(false)
    , outputBufferWasFull_(false) {
        readSize();
    }

    size_t write(uint8_t);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    int read();
    int peek();
    void flush();
    size_t read(uint8_t *buf, size_t size);
    // size_t readBytes(char *buffer, size_t length) { return read((uint8_t *)buffer, length); }

    bool seek(uint32_t pos, SeekMode mode);
    // bool seek(uint32_t pos) { return seek(pos, SeekSet); }
    size_t position() const;
    size_t size() const;
    void close();
    operator bool() const;
    time_t getLastWrite();
    const char *path() const;
    const char *name() const;

    boolean isDirectory(void);
    File openNextFile(const char *mode = FILE_READ);
    void rewindDirectory(void);

protected:
    File compressedFile_;
    size_t position_;
    GUnzip gunzip_;
    uint32_t uncompressedSize_;
    uint8_t inBuf_[1024];
    int inSize_;
    int inIndex_;
    uint8_t outBuf_[1024];
    int outSize_;
    int outIndex_;
    bool outputBufferWasFull_;
    bool lastInput_;

    size_t readSize();
};