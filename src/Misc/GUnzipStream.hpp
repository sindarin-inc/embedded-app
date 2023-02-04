#pragma once

#include "sindarin-debug.h"
#include "FS.h"
#include "Renderers/RendererStream.hpp"
#include "Misc/GUnzip.hpp"

class GUnzipStream : public RendererStream {
public:
    GUnzipStream(File gzFile) : position_(0), uncompressedSize_(0), gzFile_(gzFile) {
        readSize();
    };
    ~GUnzipStream() {};

    auto available() -> int;
    auto readBytes(uint8_t* buf, int size) -> int;
    auto read() -> int;
    auto seek(uint32_t pos) -> bool;
    auto position() -> size_t;
    auto size() -> size_t;

private:

    size_t position_;
    uint32_t uncompressedSize_;
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