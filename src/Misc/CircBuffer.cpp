#include "CircBuffer.hpp"

#include <algorithm>
#include <cstring>

#include "sindarin-debug.h"

void CircBuffer::push(uint8_t *input, int size) {
    if ((size_ + size) > capacity_) {
        // Calculate how much the buffer will overflow
        int overflowAmount = (size_ + size) - capacity_;

        // Move the start position forward to make room for the new data, effectively
        // discarding the oldest data
        startPos_ = (startPos_ + overflowAmount) % capacity_;

        // Adjust the size of the buffer if necessary
        size_ = capacity_ - overflowAmount;
    }

    int insertPoint = (startPos_ + size_) % capacity_;

    int bytesCopied = 0;
    while (bytesCopied < size) {
        int bytesToCopy = std::min(size - bytesCopied, capacity_ - insertPoint);
        memcpy(&buf_[insertPoint], &input[bytesCopied], bytesToCopy);
        bytesCopied += bytesToCopy;
        insertPoint = (insertPoint + bytesToCopy) % capacity_;
    }

    // Update the size of the buffer, but ensure it does not exceed the capacity
    size_ = std::min(size_ + size, capacity_);
}

void CircBuffer::copyFrom(int pos, uint8_t *output, int size) {
    if (pos + size > size_) {
        // error
        log_e("CircBuffer error: tried to copy %d bytes from pos %d buffer of size %d", size, pos,
              size_);
        return;
    }

    int copyPoint = (startPos_ + pos) % capacity_;

    int bytesCopied = 0;
    while (bytesCopied < size) {
        int bytesToCopy = std::min(size - bytesCopied, capacity_ - copyPoint);
        memcpy(&output[bytesCopied], &buf_[copyPoint], bytesToCopy);
        bytesCopied += bytesToCopy;
        copyPoint = (copyPoint + bytesToCopy) % capacity_;
    }
}

void CircBuffer::moveFrom(int pos, uint8_t *output, int size) {
    if (size > size_) {
        // Trying to move more data than exists, potential error handling
        log_e("CircBuffer error: tried to move %d bytes from buffer of size %d", size, size_);
        return;
    }

    copyFrom(pos, output, size);

    // Move the start position forward by 'size' and reduce the buffer size accordingly
    startPos_ = (startPos_ + size) % capacity_;
    size_ -= size;
}

void CircBuffer::clear() {
    size_ = 0;
    startPos_ = 0;
}
