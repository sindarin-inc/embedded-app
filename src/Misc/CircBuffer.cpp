#include "CircBuffer.hpp"

#include <cstring>

void CircBuffer::push(uint8_t *input, int size) {
    if (size_ + size > capacity_) {
        int diff = size_ + size - capacity_;
        startPos_ += diff;
        size_ -= diff;
        if (startPos_ >= capacity_) {
            startPos_ -= capacity_;
        }
    }

    int insertPoint = startPos_ + size_;
    if (insertPoint >= capacity_) {
        insertPoint -= capacity_;
    }

    int bytesCopied = 0;
    while (bytesCopied < size) {
        int bytesToCopy = size - bytesCopied;
        if (insertPoint + bytesToCopy > capacity_) {
            bytesToCopy = capacity_ - insertPoint;
        }

        // copy data
        memcpy(&buf_[insertPoint], &input[bytesCopied], bytesToCopy);
        size_ += bytesToCopy;
        bytesCopied += bytesToCopy;
        insertPoint += bytesToCopy;
        if (insertPoint >= capacity_) {
            insertPoint -= capacity_;
        }
    }
}

void CircBuffer::copyFrom(int pos, uint8_t *output, int size) {
    if (pos + size > size_) {
        // error
        return;
    }

    int copyPoint = startPos_ + pos;
    if (copyPoint >= capacity_) {
        copyPoint -= capacity_;
    }

    int bytesCopied = 0;
    while (bytesCopied < size) {
        int bytesToCopy = size - bytesCopied;
        if (copyPoint + bytesToCopy > capacity_) {
            bytesToCopy = capacity_ - copyPoint;
        }

        memcpy(&output[bytesCopied], &buf_[copyPoint], bytesToCopy);
        bytesCopied += bytesToCopy;
        copyPoint += bytesToCopy;
        if (copyPoint >= capacity_) {
            copyPoint -= capacity_;
        }
    }
}

void CircBuffer::clear() {
    size_ = 0;
    startPos_ = 0;
}