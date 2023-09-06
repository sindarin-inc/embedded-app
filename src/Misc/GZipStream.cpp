#include "GZipStream.hpp"

auto GZipStream::init() -> bool {
    memset(&_zs, 0, sizeof(_zs));

    if (deflateInit2(&_zs, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    return true;
}

auto GZipStream::compressBlock(int blockSize, uint8_t *outputBuffer, int *outputBufferLength,
                               int outputBufferSize, bool flush) -> bool {
    _zs.next_in = _chunkBuffer;
    _zs.avail_in = blockSize;

    int ret;
    *outputBufferLength = 0;
    uint8_t *out = outputBuffer;
    int outputSizeRemaining = outputBufferSize;

    log_i("compressBlock input size %d flush %d osr %d", blockSize, flush, outputSizeRemaining);

    int flush_type = flush ? Z_FINISH : Z_NO_FLUSH;
    // retrieve the compressed bytes blockwise
    do {
        _zs.next_out = out;
        _zs.avail_out = outputSizeRemaining;

        ret = deflate(&_zs, flush_type);
        if (ret != Z_OK) {
            log_e("zlib deflate failed: %d %s", ret, _zs.msg);
            return false;
        }

        if (_zs.total_out > 0) {
            out += _zs.total_out;
            *outputBufferLength += _zs.total_out;
            outputSizeRemaining -= _zs.total_out;
        }
    } while (_zs.avail_in != 0);

    log_i("compressBlock got bytes %d", *outputBufferLength);

    if (flush) {
        deflateEnd(&_zs);
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