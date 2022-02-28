#pragma once

#include <cinttypes>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

class RendererStream {
public:
    virtual ~RendererStream() = default;

    virtual auto available() -> int = 0;
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

    auto available() -> int override { return stream_.peek() != EOF; }
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
