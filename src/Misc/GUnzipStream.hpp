#pragma once

#include <utility>

#include "Misc/GUnzip.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/SFile.hpp"
#include "sindarin-debug.h"

#define GZ_BUFFER_SIZE 128
// LittleFS seems to have an issue where reads of > 128 bytes sometimes fail.
// This equates to a difference between using read() and fread(), and can be seen in
// vfs_api.cpp read() function.
// Avoiding this issue here by keeping input/output buffers to 128 bytes

class GUnzipStream : public RendererStream {
public:
    GUnzipStream(SFile gzFile, StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), gzFile_(std::move(gzFile)) {
        readSize();
    }
    ~GUnzipStream() override { gzFile_.close(); };

    auto available() -> int override;
    auto readBytes(uint8_t *buf, int size) -> int override;
    auto read() -> int override;
    auto seek(uint32_t pos) -> bool override;
    auto position() -> size_t override;
    auto size() -> size_t override;

private:
    size_t position_{0};
    uint32_t uncompressedSize_{0};
    SFile gzFile_;
    GUnzip gunzip_;
    uint8_t inBuf_[GZ_BUFFER_SIZE];
    int inSize_{0};
    int inIndex_{0};
    uint8_t outBuf_[GZ_BUFFER_SIZE];
    int outSize_{0};
    int outIndex_{0};
    bool lastInput_{false};

    void readSize();
};
