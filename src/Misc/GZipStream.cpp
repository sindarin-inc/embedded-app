#include "GZipStream.hpp"

//#include "SpiramAllocator.hpp"
//#include "sindarin-debug.h"

bool GZipStream::init() {
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    return true;
}

auto GZipStream::compressBlock(int blockSize, uint8_t *outputBuffer, int *outputBufferLength,
                               int outputBufferSize, bool flush) -> bool {
    // z_stream zs;
    // memset(&zs, 0, sizeof(zs));

    // if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    //     return false;
    // }

    // init();

    printf("compressing block size: %d\n", blockSize);

    zs.next_in = _chunkBuffer;
    zs.avail_in = blockSize;

    int ret;
    *outputBufferLength = 0;
    uint8_t *out = outputBuffer;
    int outputSizeRemaining = outputBufferSize;

    int flush_type = flush ? Z_FINISH : Z_NO_FLUSH;
    // retrieve the compressed bytes blockwise

    /*
    do {
        zs.next_out = out;
        zs.avail_out = outputSizeRemaining;

        printf("1: ai %d ao %d\n", zs.avail_in, zs.avail_out);

        ret = deflate(&zs, flush_type);
        printf("2: ai %d ao %d to %d\n", zs.avail_in, zs.avail_out, (int)zs.total_out);
        if (zs.total_out > 0) {
            out += zs.total_out;
            *outputBufferLength += zs.total_out;
            outputSizeRemaining -= zs.total_out;
        }
    } while (ret == Z_OK);
    */

    /*
    zs.next_out = out;
    zs.avail_out = outputSizeRemaining;

    printf("1: ai %d ao %d\n", zs.avail_in, zs.avail_out);

    ret = deflate(&zs, flush_type);
    printf("2: ai %d ao %d to %d\n", zs.avail_in, zs.avail_out, (int)zs.total_out);
    if (zs.total_out > 0) {
        out += zs.total_out;
        *outputBufferLength += zs.total_out;
        outputSizeRemaining -= zs.total_out;
    }
    */

    do {
        zs.next_out = out;
        zs.avail_out = outputSizeRemaining;

        printf("1: ai %d ao %d\n", zs.avail_in, zs.avail_out);

        ret = deflate(&zs, flush_type);
        printf("ret: %d\n", ret);
        printf("2: ai %d ao %d to %d\n", zs.avail_in, zs.avail_out, (int)zs.total_out);
        /*
        if (zs.total_out > 0) {
            out += zs.total_out;
            *outputBufferLength += zs.total_out;
            outputSizeRemaining -= zs.total_out;
        }
        */

        int compressedBytes = outputSizeRemaining - zs.avail_out;
        if (compressedBytes > 0) {
            out += compressedBytes;
            *outputBufferLength += compressedBytes;
            outputSizeRemaining -= compressedBytes;
        }
    } while (zs.avail_in != 0);

    if (zs.avail_in != 0) {
        printf("compress error: not all input bytes consumed %d\n", zs.avail_in);
    }

    if (flush) {
        deflateEnd(&zs);
    }

    /*
    if (ret != Z_STREAM_END) {
        printf("error: %d %s\n", ret, zs.msg);
        return NULL;
    }
    */

    printf("compress: %d bytes\n", *outputBufferLength);

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

    // printf("write %d\n", dataToCompressLength);

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