#include "CircBuffer.hpp"

#include <algorithm>
#include <cstring>

#include "sindarin-debug.h"

void CircBuffer::push(uint8_t *input, int size) {
    if ((size_ + size) > capacity_) {
        int diff = size_ + size - capacity_;

        // remove data from beginning of buffer
        size_ -= diff;
        startPos_ = (startPos_ + diff) % capacity_;
    }

    int insertPoint = (startPos_ + size_) % capacity_;

    int bytesCopied = 0;
    while (bytesCopied < size) {
        int bytesToCopy = std::min(size - bytesCopied, capacity_ - insertPoint);

        // copy data
        memcpy(&buf_[insertPoint], &input[bytesCopied], bytesToCopy);
        size_ += bytesToCopy;
        bytesCopied += bytesToCopy;
        insertPoint = (insertPoint + bytesToCopy) % capacity_;
    }
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

void CircBuffer::clear() {
    size_ = 0;
    startPos_ = 0;
}