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
    // todo: return available bytes.
    // this is size - position
    return 0;
}
int GUnzipFile::read() {
    // todo: return next byte in decompressed buffer
    // and advance position by one.
    uint8_t byte;
    size_t retval = read(&byte, 1);
    if (retval != 1) {
        return -1;
    }
    return byte;
}
int GUnzipFile::peek() {
    // todo: return next byte in decompressed buffer
    return 0;
}
void GUnzipFile::flush() {}
size_t GUnzipFile::read(uint8_t *buf, size_t size) {
    // todo: read from uncompressed buffer. Keep decompressing until
    // enough bytes have been read.
    // temp
    memset(buf, 0, size);
    //log_i("GUNzip file read %d\n", size);

    int bytesCopied = 0;
    while (size - bytesCopied > 0) {
        //log_i("gunzip bc %d\n", bytesCopied);
        // first check if we have any uncompressed bytes available
        if (outSize_ > 0) {
            //log_i("out size %d\n", outSize_);
            // copy as many bytes as possible
            int bytesToCopy = size - bytesCopied;
            if (bytesToCopy > outSize_) {
                bytesToCopy = outSize_;
            }

            //log_i("copying %d\n", bytesToCopy);

            memcpy(&buf[bytesCopied], &outBuf_[outIndex_], bytesToCopy);

            //log_i("output now: ****\n%s\n*****\n", buf);

            outSize_ -= bytesToCopy;
            outIndex_ += bytesToCopy;

            bytesCopied += bytesToCopy;

            //log_i("outsize now %d\n", outSize_);

            if (outSize_ > 0) {
                continue;
            }

            // output buffer empty now
            // if output buffer was full before, decompress again to flush internal buffer
            if (outputBufferWasFull_) {
                outputBufferWasFull_ = false;

                int inputRead = 0;

                memset(outBuf_, 0, 1024);

                int err = gunzip_.decompress(inBuf_, 0, outBuf_, 1024, inputRead, outSize_);
                //log_i("flush got %d\n", outSize_);
                //log_i("output buffer now: ***%s***\n", outBuf_);

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
                log_i("last input\n");
                return bytesCopied;
            }

            // try to read more input data
            int bytesRead = compressedFile_.readBytes((char*)inBuf_, 1024);
            if (bytesRead < 1024) {
                // done reading
                lastInput_ = true;
            }

            if (bytesRead == 0) {
                log_i("read 0\n");
                return bytesCopied;
            }

            inSize_ = bytesRead;
            inIndex_ = 0;
        }

        // some data available to decompress
        int inputRead = 0;
        //log_i("dec %d %d outsize %d\n", inIndex_, inSize_, outSize_);
        int err = gunzip_.decompress(&inBuf_[inIndex_], inSize_, outBuf_, 1024, inputRead, outSize_);
        //log_i("dec result: %d bytes\n", outSize_);

        if (err != Z_OK && err != Z_STREAM_END) {
            log_i("tried to decompress %d bytes index %d\n", inSize_, inIndex_);
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

    return false;
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