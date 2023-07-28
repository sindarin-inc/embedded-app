#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "Misc/SpiramAllocator.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

enum class StreamType : uint8_t { TXT, EPUB, OTHER };

class RendererStream {
private:
    StreamType streamType_{StreamType::OTHER};

protected:
    RendererStream(StreamType streamType) : streamType_(streamType) {}
    RendererStream() = default;

public:
    virtual ~RendererStream() = default;

    [[nodiscard]] auto isA(StreamType streamType) const -> bool {
        return streamType == streamType_;
    }
    [[nodiscard]] auto getStreamType() const -> StreamType { return streamType_; }

    static auto parseStreamType(const char *path) -> StreamType {
        if (HasSuffix(path, ".txt") || HasSuffix(path, ".txt.gz")) {
            return StreamType::TXT;
        }
        if (HasSuffix(path, ".epub")) {
            return StreamType::EPUB;
        }
        return StreamType::OTHER;
    }

    virtual auto getNextByte() -> uint8_t {
        if (size() > position()) {
            return static_cast<uint8_t>(read());
        } else {
            return 0;
        }
    }

    virtual auto available() -> int { return size() - position(); };
    virtual auto readBytes(uint8_t *buf, int size) -> int {
        // implementation of byte-by-byte read for a block,
        // only used if RendererStream subclass does not override this
        // currently only compressed streams use and override this method
        int ch = 0;
        for (int i = 0; i < size; i++) {
            ch = read();
            if (ch < 0) {
                return i;
            }

            buf[i] = ch;
        }

        return size;
    };
    virtual auto read() -> int = 0;
    virtual auto seek(uint32_t pos) -> bool = 0;
    virtual auto position() -> size_t = 0;
    virtual auto size() -> size_t = 0;
};

template <typename T>
class ContainerStream : public RendererStream {
public:
    ContainerStream(StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), contained_(T()) {}
    ContainerStream(T contained, StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), contained_(std::move(contained)) {}

    auto available() -> int override { return contained_.available(); }
    auto read() -> int override { return contained_.read(); };
    auto readBytes(uint8_t *buf, int size) -> int override {
        return contained_.readBytes((char *)buf, size);
    }
    auto seek(uint32_t pos) -> bool override { return contained_.seek(pos); };
    auto position() -> size_t override { return contained_.position(); }
    auto size() -> size_t override { return contained_.size(); }

protected:
    T contained_;
};

class StdStringStream : public RendererStream {
public:
    StdStringStream(std::string &string, StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), stream_(std::make_shared<std::istringstream>(string)) {
        setSize(string.length());
    }
    StdStringStream(SpiramString &string, StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), stream_(std::make_shared<SpiramIStream>(string)) {
        setSize(string.length());
    }

    void setSize(size_t size) { size_ = size; }

    auto available() -> int override { return size() - position(); }
    auto read() -> int override { return stream_->get(); };
    auto seek(uint32_t pos) -> bool override {
        stream_->seekg(pos);
        return stream_->good();
    };
    auto position() -> size_t override { return stream_->tellg(); }
    auto size() -> size_t override { return size_; }

protected:
    StdStringStream(std::shared_ptr<std::istream> stream, StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), stream_(std::move(stream)) {}

    std::shared_ptr<std::istream> stream_;
    size_t size_ = 0;
};

class BufferStream : public RendererStream {
public:
    BufferStream(const uint8_t *data, int size, StreamType streamType = StreamType::TXT)
        : RendererStream(streamType), data_(data), size_(size) {}

    auto read() -> int override { return data_[pos_++]; };
    auto readBytes(uint8_t *buf, int size) -> int override {
        int readSize = size;
        if (available() < readSize) {
            readSize = available();
        }

        if (readSize == 0) {
            return 0;
        }

        memcpy(buf, &data_[pos_], readSize);
        pos_ += readSize;
        return readSize;
    }
    auto seek(uint32_t pos) -> bool override {
        if (pos_ < 0 || pos >= size_) {
            return false;
        }

        pos_ = pos;
        return true;
    }
    auto position() -> size_t override { return pos_; }
    auto size() -> size_t override { return size_; }

protected:
    const uint8_t *data_;
    int pos_ = 0;
    size_t size_ = 0;
};
