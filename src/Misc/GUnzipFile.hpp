#pragma once

#include "FS.h"
#include "FSImpl.h"
#include "Misc/GUnzip.hpp"

class GUnzipFile : public fs::FileImpl {
public:
    GUnzipFile(File compressedFile) 
    : compressedFile_(compressedFile)
    , position_(0) 
    , inSize_(0)
    , inIndex_(0)
    , outSize_(0)
    , outIndex_(0)
    , lastInput_(false)
    , outputBufferWasFull_(false)
    {
        readSize();
    }

    size_t write(const uint8_t *buf, size_t size) override;
    size_t read(uint8_t *buf, size_t size) override;
    void flush() override;
    bool seek(uint32_t pos, SeekMode mode) override;
    size_t position() const override;
    size_t size() const override;
    void close() override;
    const char *name() const override;
    const char *path() const override;
    time_t getLastWrite() override;
    boolean isDirectory(void) override;
    fs::FileImplPtr openNextFile(const char *mode) override;
    void rewindDirectory(void) override;
    operator bool() override;

    static auto OpenGzFile(File compressedFile) -> File;

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
    bool lastInput_;
    bool outputBufferWasFull_;

    void readSize();
};