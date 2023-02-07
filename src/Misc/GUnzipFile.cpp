#include "GUnzipFile.hpp"

#include <algorithm>

auto GUnzipFile::write(const uint8_t *buf, size_t size) -> size_t {
    // N/A, this is a readonly file
    log_w("write() called on readonly gzip file!");
    assert(false);
    return 0;
}

auto GUnzipFile::read(uint8_t *buf, size_t size) -> size_t {
    // read from uncompressed buffer. Keep decompressing until
    // enough bytes have been read.
    int bytesCopied = 0;
    while (size - bytesCopied > 0) {
        // first check if we are reading from the lookback buffer
        if (lookBackBytes_ > 0) {

            int bytesToCopy = std::min((int)size - bytesCopied, lookBackBytes_);

            if (buf) {
                lookBackBuffer_.copyFrom(lookBackBuffer_.size() - lookBackBytes_, &buf[bytesCopied],
                                         bytesToCopy);
            }

            lookBackBytes_ -= bytesToCopy;
            bytesCopied += bytesToCopy;
            position_ += bytesToCopy;

            continue;
        }

        // next check if we have any uncompressed bytes available
        if (outSize_ > 0) {
            // copy as many bytes as possible
            int bytesToCopy = std::min((int)size - bytesCopied, outSize_);

            if (buf) {
                memcpy(&buf[bytesCopied], &outBuf_[outIndex_], bytesToCopy);
            }

            // copy bytes into circular buffer
            lookBackBuffer_.push(&outBuf_[outIndex_], bytesToCopy);

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
            int inputRead = 0;
            int err = gunzip_.decompress(inBuf_, 0, outBuf_, 1024, inputRead, outSize_);
            if (err == Z_BUF_ERROR) {
                // no more output to flush
                outSize_ = 0;
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
            int bytesRead = compressedFile_.readBytes((char *)inBuf_, 1024);
            if (bytesRead < 1024) {
                // done reading
                lastInput_ = true;
            }

            if (bytesRead == 0) {
                return bytesCopied;
            }

            inSize_ = bytesRead;
            inIndex_ = 0;
        }

        // some data available to decompress
        int inputRead = 0;
        int err =
            gunzip_.decompress(&inBuf_[inIndex_], inSize_, outBuf_, 1024, inputRead, outSize_);

        if (err != Z_OK && err != Z_STREAM_END) {
            log_e("GUNzipFile: Error decompressing %d", err);
            return 0;
        }

        inSize_ -= inputRead;
        inIndex_ += inputRead;
        outIndex_ = 0;
    }

    return bytesCopied;
}

void GUnzipFile::flush() {
    // N/A, readonly file
}

auto GUnzipFile::seek(uint32_t pos, SeekMode mode) -> bool {
    size_t newPosition = 0;
    if (mode == SeekSet) {
        newPosition = pos;
    } else if (mode == SeekCur) {
        newPosition = position_ + pos;
    } else if (mode == SeekEnd) {
        // not supported yet (pos is uint32_t? arg needs to be negative?)
        newPosition = position_;
    }

    if (newPosition > position_) {
        // for forwards seeks, decompress until desired location is reached.
        int offset = newPosition - position_;
        read(nullptr, offset);
        position_ = newPosition;
    } else if (newPosition < position_) {
        // need to start over from beginning of the file
        // and decompress to new position
        // TODO: implement table of seek indices for faster seeking

        // see if lookback buffer has enough bytes to seek back
        int offset = position_ - newPosition;
        if (lookBackBytes_ + offset <= lookBackBuffer_.size()) {
            // able to seek within the lookback buffer
            lookBackBytes_ += offset;
        } else {
            // seek file back to beginning
            lookBackBuffer_.clear();
            lookBackBytes_ = 0;
            compressedFile_.seek(0, SeekSet);
            inSize_ = 0;
            inIndex_ = 0;
            outSize_ = 0;
            outIndex_ = 0;
            gunzip_.init();

            // decompress enough to get to the new position
            read(nullptr, newPosition);
        }

        position_ = newPosition;
    }

    return true;
}
auto GUnzipFile::position() const -> size_t { return position_; }

void GUnzipFile::readSize() {
    // get compressed file length
    size_t gzLen = compressedFile_.size();

    // uncompressed length is in the last 4 bytes of the gz file
    // seek there
    compressedFile_.seek(gzLen - 4, SeekSet);

    // read uncompressed file length
    uint8_t sizeBytes[4];
    compressedFile_.readBytes((char *)sizeBytes, 4);

    // rewind file
    compressedFile_.seek(0, SeekSet);

    // move bytes (size is little endian)
    uncompressedSize_ =
        (sizeBytes[3] << 24) + (sizeBytes[2] << 16) + (sizeBytes[1] << 8) + sizeBytes[0];
}

auto GUnzipFile::size() const -> size_t { return uncompressedSize_; }

void GUnzipFile::close() { compressedFile_.close(); }

auto GUnzipFile::name() const -> const char * { return compressedFile_.name(); }
auto GUnzipFile::path() const -> const char * { return compressedFile_.path(); }
auto GUnzipFile::getLastWrite() -> time_t { return compressedFile_.getLastWrite(); }
auto GUnzipFile::isDirectory() -> boolean { return compressedFile_.isDirectory(); }
auto GUnzipFile::openNextFile(const char *mode) -> fs::FileImplPtr { return nullptr; }
void GUnzipFile::rewindDirectory() {}
GUnzipFile::operator bool() {
    if (compressedFile_) {
        return true;
    }
    return false;
}

auto GUnzipFile::openGzFile(const File &compressedFile) -> File {
    auto file = File(std::make_shared<GUnzipFile>(compressedFile));
    return file;
}