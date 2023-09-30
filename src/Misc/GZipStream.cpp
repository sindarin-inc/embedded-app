#include "GZipStream.hpp"

#include "sindarin-debug.h"

#define MAX_GZIP_CHUNK_SIZE 32768

static auto spiZAlloc(void *opaque, size_t items, size_t size) -> void * {
    return SpiRamMalloc(items * size);
}
static void spiZFree(void *opaque, void *address) { SpiRamFree(address); }

auto GZipStream::init() -> bool {
    memset(&zs_, 0, sizeof(zs_));

    zs_.zalloc = spiZAlloc;
    zs_.zfree = spiZFree;

    int ret = mz_deflateInit2(&zs_, MZ_BEST_COMPRESSION, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 8,
                              MZ_DEFAULT_STRATEGY);
    if (ret != MZ_OK) {
        log_e("error in mz_deflateInit2: %d", ret);
        return false;
    }

    outputBuffer_ = SpiRamArrayPtr<uint8_t>(MAX_GZIP_CHUNK_SIZE + 10);

    // writing gz header
    // fixed 10 bytes
    uint8_t *ob = outputBuffer_.get();
    ob[0] = 0x1F;
    ob[1] = 0x8B;
    ob[2] = 8;
    ob[3] = 0;
    ob[4] = 0;
    ob[5] = 0;
    ob[6] = 0;
    ob[7] = 0;
    ob[8] = 0;
    ob[9] = 0xFF;

    outputAvailable_ = 10;

    init_ = true;

    return true;
}

auto GZipStream::compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                               int *outputBufferLength, int outputBufferSize, bool flush,
                               bool &done) -> bool {
    done = false;

    // calculate crc
    if (blockSize > 0) {
        crc_ = mz2_crc32(crc_, inputBuffer, blockSize);
    }

    zs_.next_in = inputBuffer;
    zs_.avail_in = blockSize;

    int ret;
    *outputBufferLength = 0;
    uint8_t *out = outputBuffer;
    int outputSizeRemaining = outputBufferSize;

    int flushType = flush ? MZ_FINISH : MZ_NO_FLUSH;

    do {
        zs_.next_out = out;
        zs_.avail_out = outputSizeRemaining;

        ret = mz_deflate(&zs_, flushType);
        if (flush) {
            if (ret == MZ_STREAM_END) {
                // nothing left to flush
                done = true;
            } else if (ret != MZ_OK) {
                log_e("error flushing gzip stream: %d %s", ret, zs_.msg);
                return false;
            }
        } else {
            if (ret != MZ_OK) {
                log_e("error in zlib deflate: %d %s", ret, zs_.msg);
                return false;
            }

            if (zs_.avail_in == 0) {
                done = true;
            }
        }

        int compressedBytes = outputSizeRemaining - zs_.avail_out;
        if (compressedBytes > 0) {
            out += compressedBytes;
            *outputBufferLength += compressedBytes;
            outputSizeRemaining -= compressedBytes;
        }
    } while ((zs_.avail_in != 0) && (outputSizeRemaining > 0) && !flush);

    if (flush && done) {
        mz_deflateEnd(&zs_);
    }

    uncompressedSize_ += blockSize;

    return true;
}

auto GZipStream::write(uint8_t *dataToCompress, int dataToCompressLength) -> bool {
    return write(dataToCompress, dataToCompressLength, false);
}

auto GZipStream::write(uint8_t *dataToCompress, int dataToCompressLength, bool flush) -> bool {
    int compressedLength = 0;
    uint8_t *outputBuffer = &outputBuffer_.get()[outputAvailable_];
    bool done;
    bool ret = compressBlock(dataToCompress, dataToCompressLength, outputBuffer, &compressedLength,
                             MAX_GZIP_CHUNK_SIZE - outputAvailable_, flush, done);
    if (!ret || !done) {
        log_e("error compressing %d bytes", dataToCompressLength);
        return false;
    }

    outputAvailable_ += compressedLength;
    return true;
}

auto GZipStream::flush() -> bool {
    bool ret = write(nullptr, 0, true);
    if (!ret) {
        return false;
    }

    // write footer
    uint8_t *f = &outputBuffer_.get()[outputAvailable_];

    f[0] = crc_ & 0xFF;
    f[1] = (crc_ >> 8) & 0xFF;
    f[2] = (crc_ >> 16) & 0xFF;
    f[3] = (crc_ >> 24) & 0xFF;
    f[4] = uncompressedSize_ & 0xFF;
    f[5] = (uncompressedSize_ >> 8) & 0xFF;
    f[6] = (uncompressedSize_ >> 16) & 0xFF;
    f[7] = (uncompressedSize_ >> 24) & 0xFF;

    outputAvailable_ += 8;

    return true;
}

auto GZipStream::read(uint8_t *destBuffer, int destBufferSize) -> int {
    int bytesToRead = destBufferSize < outputAvailable_ ? destBufferSize : outputAvailable_;
    memcpy(destBuffer, outputBuffer_.get(), bytesToRead);

    advance(bytesToRead);
    return bytesToRead;
}

auto GZipStream::getDataPtr() -> uint8_t * { return outputBuffer_.get(); }

void GZipStream::advance(int size) {
    if (size < outputAvailable_) {
        int bytesRemaining = outputAvailable_ - size;
        memmove(outputBuffer_.get(), &outputBuffer_.get()[size], bytesRemaining);
        outputAvailable_ -= size;
    } else {
        outputAvailable_ = 0;
    }
}