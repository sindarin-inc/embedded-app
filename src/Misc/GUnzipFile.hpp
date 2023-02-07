#pragma once

#include <utility>

#include "FS.h"
#include "FSImpl.h"
#include "Misc/CircBuffer.hpp"
#include "Misc/GUnzip.hpp"

#define LOOKBACK_SIZE 32768

class GUnzipFile : public fs::FileImpl {
public:
    GUnzipFile(File compressedFile)
        : compressedFile_(std::move(std::move(compressedFile))), lookBackBuffer_(LOOKBACK_SIZE)

    {
        readSize();
    }

    auto write(const uint8_t *buf, size_t size) -> size_t override;
    auto read(uint8_t *buf, size_t size) -> size_t override;
    void flush() override;
    auto seek(uint32_t pos, SeekMode mode) -> bool override;
    [[nodiscard]] auto position() const -> size_t override;
    [[nodiscard]] auto size() const -> size_t override;
    void close() override;
    [[nodiscard]] auto name() const -> const char * override;
    [[nodiscard]] auto path() const -> const char * override;
    auto getLastWrite() -> time_t override;
    auto isDirectory() -> boolean override;
    auto openNextFile(const char *mode) -> fs::FileImplPtr override;
    void rewindDirectory() override;
    operator bool() override;

    static auto openGzFile(const File &compressedFile) -> File;

protected:
    File compressedFile_;
    size_t position_{0};
    GUnzip gunzip_;
    uint32_t uncompressedSize_;
    uint8_t inBuf_[1024];
    int inSize_{0};
    int inIndex_{0};
    uint8_t outBuf_[1024];
    int outSize_{0};
    int outIndex_{0};
    bool lastInput_{false};
    CircBuffer lookBackBuffer_;
    int lookBackBytes_{0};

    void readSize();
};