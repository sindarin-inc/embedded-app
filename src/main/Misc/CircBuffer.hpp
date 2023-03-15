#pragma once

#include <cstdint>
#include <memory>

class CircBuffer {
public:
    CircBuffer(int size) : capacity_(size) { buf_ = std::make_unique<uint8_t[]>(size); }

    void push(uint8_t *input, int size);
    void copyFrom(int pos, uint8_t *output, int size);
    void clear();

    auto size() -> int { return size_; }
    auto capacity() -> int { return capacity_; }

private:
    std::unique_ptr<uint8_t[]> buf_;
    int startPos_{0};
    int size_{0};
    int capacity_;
};