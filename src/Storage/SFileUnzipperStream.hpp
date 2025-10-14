#pragma once

#include <Unzipper/UnzipperFileStream.hpp>
#include <memory>

#include "Storage/SFile.hpp"

// Compatibility layer to hook up a SFile to an UnzipperFileStream
class SFileUnzipperStream : public UnzipperFileStream {
public:
    explicit SFileUnzipperStream(const SFile &file) : file_(file) {}
    ~SFileUnzipperStream() override = default;

    explicit operator bool() const override { return static_cast<bool>(file_); }

    void close() override { file_.close(); }

    auto read(uint8_t *buf, size_t size) -> size_t override { return file_.read(buf, size); }

    auto seek(off_t offset, EPFSSeekWhence whence) -> bool override {
        SSeekMode mode = SSeekSet;
        switch (whence) {
        case EPFSSeekSet:
            mode = SSeekSet;
            break;
        case EPFSSeekCur:
            mode = SSeekCur;
            break;
        case EPFSSeekEnd:
            mode = SSeekEnd;
            break;
        }
        return file_.seek(static_cast<uint32_t>(offset), mode);
    }

    auto position() -> size_t override { return file_.position(); }

private:
    SFile file_;
};
