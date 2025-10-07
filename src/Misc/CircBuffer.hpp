#pragma once

#include <Spiram/Memory.hpp>
#include <cstdint>
#include <memory>

class CircBuffer {
public:
    CircBuffer(int size, bool useSpiram = false) : capacity_(size) {
        if (useSpiram) {
            buf_ = SpiramArrayPtr<uint8_t>(size);
        } else {
            buf_ = std::shared_ptr<uint8_t[]>(new uint8_t[size]);
        }
    }

    void push(const uint8_t *input, int size);
    void copyFrom(int pos, uint8_t *output, int size);
    void moveFrom(int pos, uint8_t *output, int size);
    void clear();

    [[nodiscard]] auto size() const -> int { return size_; }
    [[nodiscard]] auto capacity() const -> int { return capacity_; }

private:
    std::shared_ptr<uint8_t[]> buf_;
    int startPos_{0};
    int size_{0};
    int capacity_;
};
