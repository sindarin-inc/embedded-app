#include "GUnzipStream.hpp"

auto GUnzipStream::available() -> int { return uncompressedSize_ - position_; }

auto GUnzipStream::readBytes(uint8_t *buf, int size) -> int {
    // read from uncompressed buffer. Keep decompressing until
    // enough bytes have been read.
    int bytesCopied = 0;
    while (size - bytesCopied > 0) {
        // next check if we have any uncompressed bytes available
        if (outSize_ > 0) {
            // copy as many bytes as possible
            int bytesToCopy = std::min(size - bytesCopied, outSize_);
            if (buf) {
                memcpy(&buf[bytesCopied], &outBuf_[outIndex_], bytesToCopy);
            }

            outSize_ -= bytesToCopy;
            outIndex_ += bytesToCopy;
            bytesCopied += bytesToCopy;
            position_ += bytesToCopy;

            if (outSize_ > 0) {
                continue;
            }

            // output buffer empty now
            // try to flush more output data. If we get Z_BUF_ERROR, it means the input is exhausted
            // and we will have to provide more input data to proceed.
            // at this point we are only attempting to flush remaining output data,
            // and no new input data is provided to zlib.
            if (lastOutputInBlock_) {
                lastOutputInBlock_ = false;
                outIndex_ = 0;
                continue;
            }

            int inputRead = 0;
            int err = gunzip_.decompress(nullptr, 0, outBuf_, GZ_BUFFER_SIZE, inputRead, outSize_);

            if (err == MZ_BUF_ERROR) {
                // no more output to flush
                // outSize_ = 0;
                lastOutputInBlock_ = true;
            }

            outIndex_ = 0;

            // keep going until we have copied enough bytes
            continue;
        }

        // at this point no output data is available
        // and internal decompress buffers are empty.
        // decompress some more data
        if (inSize_ == 0) {
            if (lastInput_) {
                // nothing more to read, done
                return bytesCopied;
            }

            // try to read more input data
            int bytesRead = gzFile_.readBytes(reinterpret_cast<char *>(inBuf_), GZ_BUFFER_SIZE);

            if (bytesRead < GZ_BUFFER_SIZE) {
                // done reading
                lastInput_ = true;
            }

            if (bytesRead == 0) {
                return bytesCopied;
            }

            inSize_ = bytesRead;
            inIndex_ = 0;
        }

        // passing compressed data in inBuf_ to zlib, output (if any) will be copied to outBuf_
        int inputRead = 0;
        int err = gunzip_.decompress(&inBuf_[inIndex_], inSize_, outBuf_, GZ_BUFFER_SIZE, inputRead,
                                     outSize_);

        if (err != MZ_OK && err != MZ_STREAM_END) {
            log_e("GUNzipFile: Error decompressing %d", err);
            return 0;
        }

        inSize_ -= inputRead;
        inIndex_ += inputRead;
        outIndex_ = 0;
        lastOutputInBlock_ = false;
    }

    return bytesCopied;
}

auto GUnzipStream::read() -> int {
    uint8_t c;
    int bytesRead = readBytes(&c, 1);
    if (bytesRead != 1) {
        return -1;
    }

    return c;
}

auto GUnzipStream::seek(uint32_t pos) -> bool {
    size_t newPosition = pos;
    if (newPosition > position_) {
        // for forwards seeks, decompress until desired location is reached.
        int offset = newPosition - position_;
        readBytes(nullptr, offset);
    } else if (newPosition < position_) {
        // need to start over from beginning of the file
        // and decompress to new position
        // seek file back to beginning
        log_w("GUnzipStream seek from %zu to %" PRIu32 ", reset to start", position_, pos);
        gzFile_.seek(0, SSeekSet);
        inSize_ = 0;
        inIndex_ = 0;
        outSize_ = 0;
        outIndex_ = 0;
        lastInput_ = false;
        gunzip_.init();

        // decompress enough to get to the new position
        readBytes(nullptr, newPosition);
    }

    position_ = newPosition;

    return true;
}

auto GUnzipStream::position() -> size_t { return position_; }

void GUnzipStream::readSize() {
    // get compressed file length
    size_t gzLen = gzFile_.size();

    // uncompressed length is in the last 4 bytes of the gz file
    // seek there
    gzFile_.seek(gzLen - 4, SSeekSet);

    // read uncompressed file length
    uint8_t sizeBytes[4];
    gzFile_.readBytes(reinterpret_cast<char *>(sizeBytes), 4);

    // rewind file
    gzFile_.seek(0, SSeekSet);

    // move bytes (size is little endian)
    uncompressedSize_ =
        (sizeBytes[3] << 24) + (sizeBytes[2] << 16) + (sizeBytes[1] << 8) + sizeBytes[0];
}

auto GUnzipStream::size() -> size_t { return uncompressedSize_; }
