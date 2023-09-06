#include "GZipStream.hpp"

//#include "SpiramAllocator.hpp"
#include "sindarin-debug.h"

bool GZipStream::init() {
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    return true;
}

auto GZipStream::compressBlock(int blockSize, uint8_t *outputBuffer, int *outputBufferLength,
                               int outputBufferSize, bool flush) -> bool {
    printf("compressing block size: %d\n", blockSize);

    zs.next_in = _chunkBuffer;
    zs.avail_in = blockSize;

    int ret;
    *outputBufferLength = 0;
    uint8_t *out = outputBuffer;
    int outputSizeRemaining = outputBufferSize;

    int flush_type = flush ? Z_FINISH : Z_NO_FLUSH;

    do {
        zs.next_out = out;
        zs.avail_out = outputSizeRemaining;

        ret = deflate(&zs, flush_type);
        if (flush) {
            if (ret != Z_STREAM_END) {
                log_e("error flushing gzip stream: %d %s", ret, zs.msg);
                return false;
            }
        } else {
            if (ret != Z_OK) {
                log_e("error in zlib deflate: %d %s", ret, zs.msg);
            }
        }

        int compressedBytes = outputSizeRemaining - zs.avail_out;
        if (compressedBytes > 0) {
            out += compressedBytes;
            *outputBufferLength += compressedBytes;
            outputSizeRemaining -= compressedBytes;
        }
    } while (zs.avail_in != 0);

    if (flush) {
        deflateEnd(&zs);
    }

    return true;
}

auto GZipStream::write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize) -> bool {
    return write(dataToCompress, dataToCompressLength, outputBuffer, outputBufferLength,
                 outputBufferSize, false);
}
auto GZipStream::write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool flush) -> bool {

    uint8_t *data = dataToCompress;
    uint8_t *output = outputBuffer;
    int bytesToCompress = dataToCompressLength;
    *outputBufferLength = 0;
    int outputSizeRemaining = outputBufferSize;

    do {
        // copy data into chunk buffer
        int bytesToCopy = bytesToCompress;
        if (bytesToCopy > (GZIP_CHUNK_SIZE - _bytesInBuffer)) {
            bytesToCopy = GZIP_CHUNK_SIZE - _bytesInBuffer;
        }

        if (bytesToCopy > 0) {
            memcpy(&_chunkBuffer[_bytesInBuffer], data, bytesToCopy);
            _bytesInBuffer += bytesToCopy;
            data += bytesToCopy;
            bytesToCompress -= bytesToCopy;
        }

        if (flush || _bytesInBuffer == GZIP_CHUNK_SIZE) {
            // compress one chunk
            int compressedBytes = 0;
            bool ret =
                compressBlock(_bytesInBuffer, output, &compressedBytes, outputSizeRemaining, flush);
            if (!ret) {
                printf("compressBlock failed\n");
                return false;
            }

            output += compressedBytes;
            outputSizeRemaining -= compressedBytes;
            *outputBufferLength += compressedBytes;
            _bytesInBuffer = 0;
        }
    } while (bytesToCompress > 0);

    return true;
}

auto GZipStream::flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize)
    -> bool {
    if (_bytesInBuffer > 0) {
        return write(NULL, 0, outputBuffer, outputBufferLength, outputBufferSize, true);
    }

    return true;
}