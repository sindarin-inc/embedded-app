#include "GUnzipFile.hpp"

size_t GUnzipFile::write(uint8_t) {
    // N/A, this is a readonly file
    return 0;
}
size_t GUnzipFile::write(const uint8_t *buf, size_t size) {
    // N/A, this is a readonly file
    return 0;
}
int GUnzipFile::available() {
    return size() - position();
}
int GUnzipFile::read() {
    uint8_t byte;
    size_t retval = read(&byte, 1);
    if (retval != 1) {
        return -1;
    }
    return byte;
}
int GUnzipFile::peek() {
    // todo: return next byte in decompressed buffer
    if (peek_) {
        return peekVal_;
    }

    peekVal_ = read();
    peek_ = true;
    position_--;

    return peekVal_;
}
void GUnzipFile::flush() {}
size_t GUnzipFile::read(uint8_t *buf, size_t size) {
    // read from uncompressed buffer. Keep decompressing until
    // enough bytes have been read.
    int bytesCopied = 0;
    while (size - bytesCopied > 0) {

        // use peek value first if present
        if (peek_) {
            if (buf) {
                buf[bytesCopied] = peekVal_;
            }
            bytesCopied++;
            position_++;
            peek_ = false;
            peekVal_ = 0;

            continue;
        }

        // first check if we have any uncompressed bytes available
        if (outSize_ > 0) {
            // copy as many bytes as possible
            int bytesToCopy = size - bytesCopied;
            if (bytesToCopy > outSize_) {
                bytesToCopy = outSize_;
            }

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
            // if output buffer was full before, decompress again to flush internal buffer
            if (outputBufferWasFull_) {
                outputBufferWasFull_ = false;

                int inputRead = 0;
                int err = gunzip_.decompress(inBuf_, 0, outBuf_, 1024, inputRead, outSize_);
                
                if (outSize_ == 1024) {
                    outputBufferWasFull_ = true;
                }

                outIndex_ = 0;
            }
            
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
            int bytesRead = compressedFile_.readBytes((char*)inBuf_, 1024);
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
        int err = gunzip_.decompress(&inBuf_[inIndex_], inSize_, outBuf_, 1024, inputRead, outSize_);
        
        if (err != Z_OK && err != Z_STREAM_END) {
            log_e("GUNzipFile: Error decompressing %d", err);
            return 0;
        }

        if (outSize_ == 1024) {
            outputBufferWasFull_ = true;
        }

        inSize_ -= inputRead;
        inIndex_ += inputRead;
        outIndex_ = 0;
    }

    return bytesCopied;
}

bool GUnzipFile::seek(uint32_t pos, SeekMode mode) {
    // todo: for backwards seeks, go back to beginning and decompress from
    // there.
    // for forwards seeks, decompress until desired location is reached.
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
        int offset = newPosition - position_;
        read(NULL, offset);
        position_ = newPosition;
    } else if (newPosition < position_) {
        // need to start over from beginning of the file
        // and decompress to new position
        // TODO: implement table of seek indices for faster seeking

        // seek file back to beginning
        compressedFile_.seek(0, SeekSet);
        inSize_ = 0;
        inIndex_ = 0;
        outSize_ = 0;
        outIndex_ = 0;
        gunzip_.init();

        // decompress enough to get to the new position
        read(NULL, newPosition);

        position_ = newPosition;
    }

    return true;
}
size_t GUnzipFile::position() const { return position_; }

size_t GUnzipFile::readSize() {
    // get compressed file length
    size_t gzLen = compressedFile_.size();

    // uncompressed length is in the last 4 bytes of the gz file
    // seek there
    compressedFile_.seek(gzLen - 4, SeekSet);

    // read uncompressed file length
    uint8_t sizeBytes[4];
    compressedFile_.readBytes((char*)sizeBytes, 4);

    // rewind file
    compressedFile_.seek(0, SeekSet);

    // move bytes (size is little endian)
    uncompressedSize_ = (sizeBytes[3] << 24) + (sizeBytes[2] << 16) + (sizeBytes[1] << 8) + sizeBytes[0];
}

size_t GUnzipFile::size() const {
    return uncompressedSize_;
}

void GUnzipFile::close() { compressedFile_.close(); }
GUnzipFile::operator bool() const {
    if (compressedFile_) {
        return true;
    }
    return false;
}
time_t GUnzipFile::getLastWrite() { return compressedFile_.getLastWrite(); }
//const char *GUnzipFile::path() const { return compressedFile_.path(); }
const char *GUnzipFile::name() const { return compressedFile_.name(); }

boolean GUnzipFile::isDirectory(void) { return compressedFile_.isDirectory(); }
File GUnzipFile::openNextFile(const char *mode) { return compressedFile_.openNextFile(mode); }
void GUnzipFile::rewindDirectory(void) { return compressedFile_.rewindDirectory(); }