#pragma once

#include <utility>

#include "FS.h"
#include "Misc/GUnzip.hpp"
#include "Renderers/RendererStream.hpp"
#include "sindarin-debug.h"

class GUnzipStream : public RendererStream {
public:
    GUnzipStream(File gzFile) : gzFile_(std::move(gzFile)) { readSize(); }
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
    File gzFile_;
    GUnzip gunzip_;
    uint8_t inBuf_[1024];
    int inSize_{0};
    int inIndex_{0};
    uint8_t outBuf_[1024];
    int outSize_{0};
    int outIndex_{0};
    bool lastInput_{false};

    void readSize();
};