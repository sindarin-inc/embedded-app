#pragma once

#include <cstdint>
#include <memory>

class CircBuffer {
public:
    CircBuffer(int size) : startPos_(0), size_(0), capacity_(size) {
        buf_ = std::make_unique<uint8_t[]>(size);
    }

    void push(uint8_t *input, int size);
    // void pop(uint8_t *output, int size);
    void copyFrom(int pos, uint8_t *output, int size);
    void clear();

    int size() { return size_; }

private:
    std::unique_ptr<uint8_t[]> buf_;
    int startPos_;
    int size_;
    int capacity_;
};