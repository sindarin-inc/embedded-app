#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "sindarin-debug.h"

class RendererStream {
public:
    virtual ~RendererStream() = default;

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
    ContainerStream() : contained_(T()) {}
    ContainerStream(T contained) : contained_(std::move(contained)) {}

    auto available() -> int override { return contained_.available(); }
    auto read() -> int override { return contained_.read(); };
    auto seek(uint32_t pos) -> bool override { return contained_.seek(pos); };
    auto position() -> size_t override { return contained_.position(); }
    auto size() -> size_t override { return contained_.size(); }

protected:
    T contained_;
};

class StdStringStream : public RendererStream {
public:
    StdStringStream(std::stringstream &stream) : stream_(std::move(stream)) {}
    StdStringStream(std::string &string) : stream_(string) { setSize(string.length()); }

    void setSize(size_t size) { size_ = size; }

    auto available() -> int override { return size() - position(); }
    auto read() -> int override { return stream_.get(); };
    auto seek(uint32_t pos) -> bool override {
        stream_.seekg(pos);
        return stream_.good();
    };
    auto position() -> size_t override { return stream_.tellg(); }
    auto size() -> size_t override { return size_; }

protected:
    std::stringstream stream_;
    size_t size_ = 0;
};
