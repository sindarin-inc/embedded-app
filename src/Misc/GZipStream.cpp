#include "GZipStream.hpp"

#include "sindarin-debug.h"

#define MAX_GZIP_CHUNK_SIZE 32768

static void *spiZAlloc(void *opaque, uInt items, uInt size) {
    log_d("spiZAlloc: %d %d", (int)items, (int)size);
    return SpiRamMalloc(items * size);
}
static void spiZFree(void *opaque, void *address) {
    log_d("spiZFree");
    SpiRamFree(address);
}

auto GZipStream::init() -> bool {
    memset(&zs_, 0, sizeof(zs_));

    zs_.zalloc = spiZAlloc;
    zs_.zfree = spiZFree;

    if (deflateInit2(&zs_, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    outputBuffer_ = SpiRamArrayPtr<uint8_t>(MAX_GZIP_CHUNK_SIZE);
    outputAvailable_ = 0;
    init_ = true;

    return true;
}

auto GZipStream::compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                               int *outputBufferLength, int outputBufferSize, bool flush,
                               bool &done) -> bool {
    done = false;

    zs_.next_in = inputBuffer;
    zs_.avail_in = blockSize;

    int ret;
    *outputBufferLength = 0;
    uint8_t *out = outputBuffer;
    int outputSizeRemaining = outputBufferSize;

    int flushType = flush ? Z_FINISH : Z_NO_FLUSH;

    do {
        zs_.next_out = out;
        zs_.avail_out = outputSizeRemaining;

        ret = deflate(&zs_, flushType);
        if (flush) {
            if (ret == Z_STREAM_END) {
                // nothing left to flush
                done = true;
            } else if (ret != Z_OK) {
                log_e("error flushing gzip stream: %d %s", ret, zs_.msg);
                return false;
            }
        } else {
            if (ret != Z_OK) {
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
        deflateEnd(&zs_);
    }

    return true;
}

auto GZipStream::write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool &done) -> bool {

    return compressBlock(dataToCompress, dataToCompressLength, outputBuffer, outputBufferLength,
                         outputBufferSize, false, done);
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

    log_i("cbytes %d", compressedLength);

    outputAvailable_ += compressedLength;
    return true;
}

auto GZipStream::flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize,
                       bool &done) -> bool {
    return compressBlock(nullptr, 0, outputBuffer, outputBufferLength, outputBufferSize, true,
                         done);
}

auto GZipStream::flush() -> bool { return write(NULL, 0, true); }

auto GZipStream::compressFileToMemory(SFile &file, uint8_t *outputBuffer, int outputBufferSize,
                                      int &compressedSize) -> bool {
    init();

    int fileSize = file.size();
    auto inBuf = SpiRamArrayPtr<uint8_t>(fileSize);

    // compress entire file
    file.read(inBuf.get(), fileSize);

    compressedSize = 0;
    bool done = false;
    bool ret = compressBlock(inBuf.get(), fileSize, outputBuffer, &compressedSize, outputBufferSize,
                             true, done);
    if (!ret || !done) {
        return false;
    }

    return true;
}

auto GZipStream::read(uint8_t *destBuffer, int destBufferSize) -> int {
    int bytesToRead = destBufferSize < outputAvailable_ ? destBufferSize : outputAvailable_;
    memcpy(destBuffer, outputBuffer_.get(), bytesToRead);

    if (bytesToRead < outputAvailable_) {
        int bytesRemaining = outputAvailable_ - bytesToRead;
        log_i("br %d btr %d", bytesRemaining, bytesToRead);
        memmove(outputBuffer_.get(), &outputBuffer_.get()[bytesToRead], bytesRemaining);
        outputAvailable_ -= bytesToRead;
        log_i("oa %d", outputAvailable_);
    } else {
        outputAvailable_ = 0;
    }

    return bytesToRead;
}