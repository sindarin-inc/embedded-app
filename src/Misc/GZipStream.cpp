#include "GZipStream.hpp"

//#include "SpiramAllocator.hpp"
#include "sindarin-debug.h"

// typedef voidpf (*alloc_func) OF((voidpf opaque, uInt items, uInt size));
// typedef void   (*free_func)  OF((voidpf opaque, voidpf address));

static void *spiZAlloc(void *opaque, uInt items, uInt size) { return SpiRamMalloc(items * size); }
static void spiZFree(void *opaque, void *address) { SpiRamFree(address); }

bool GZipStream::init() {
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    return true;
}

auto GZipStream::compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                               int *outputBufferLength, int outputBufferSize, bool flush,
                               bool &done) -> bool {
    printf("compressing block size: %d flush %d\n", blockSize, flush);

    done = false;

    zs.next_in = inputBuffer;
    zs.avail_in = blockSize;
    zs.zalloc = spiZAlloc;
    zs.zfree = spiZFree;

    int ret;
    *outputBufferLength = 0;
    uint8_t *out = outputBuffer;
    int outputSizeRemaining = outputBufferSize;

    int flush_type = flush ? Z_FINISH : Z_NO_FLUSH;

    do {
        zs.next_out = out;
        zs.avail_out = outputSizeRemaining;

        ret = deflate(&zs, flush_type);
        log_i("ret: %d ai %d ao %d", ret, zs.avail_in, zs.avail_out);
        if (flush) {
            if (ret == Z_STREAM_END) {
                // nothing left to flush
                done = true;
            } else if (ret != Z_OK) {
                log_e("error flushing gzip stream: %d %s", ret, zs.msg);
                return false;
            }
        } else {
            if (ret != Z_OK) {
                log_e("error in zlib deflate: %d %s", ret, zs.msg);
                return false;
            }

            if (zs.avail_in == 0) {
                done = true;
            }
        }

        int compressedBytes = outputSizeRemaining - zs.avail_out;
        if (compressedBytes > 0) {
            out += compressedBytes;
            *outputBufferLength += compressedBytes;
            outputSizeRemaining -= compressedBytes;
        }
    } while (zs.avail_in != 0 && outputSizeRemaining > 0 != 0 && !flush);

    log_i("output: %d bytes", *outputBufferLength);

    if (flush && done) {
        log_i("deflateEnd");
        deflateEnd(&zs);
    }

    return true;
}

auto GZipStream::write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool &done) -> bool {

    return compressBlock(dataToCompress, dataToCompressLength, outputBuffer, outputBufferLength,
                         outputBufferSize, false, done);
}

auto GZipStream::flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize,
                       bool &done) -> bool {
    return compressBlock(NULL, 0, outputBuffer, outputBufferLength, outputBufferSize, true, done);
}