#include "GUnzip.hpp"

#include <cstdio>
#include <memory>

#include "sindarin-debug.h"

#define BSIZE 128
#define GZ_FILE_HEADER_MASK 0x08

typedef struct __attribute__((__packed__)) {
    uint8_t magic[2];
    uint8_t compressionMethod;
    uint8_t fileFlags;
    uint32_t timestamp;
    uint8_t compressionFlags;
    uint8_t operatingSystemID;
} gzHeader;

static gzHeader gzhdr;

static auto spiZAlloc(void *opaque, size_t items, size_t size) -> void * {
    return SpiRamMalloc(items * size);
}
static void spiZFree(void *opaque, void *address) { SpiRamFree(address); }

auto GUnzip::init() -> bool {

    int err = MZ_OK;

    if (init_) {
        // starting over, so clean up existing decompressor
        err = mz_inflateEnd(&zstr_);
    }

    init_ = true;
    compressedPos_ = 0;
    uncompressedPos_ = 0;

    memset(&zstr_, 0, sizeof(mz_stream));
    memset(&gzhdr, 0, sizeof(gzHeader));
    gzHeaderBytes_ = 0;
    readHeader_ = false;

    zstr_.zalloc = spiZAlloc;
    zstr_.zfree = spiZFree;

    zstr_.avail_in = 0;
    zstr_.next_in = nullptr;

    // initialize inflate with -MZ_DEFAULT_WINDOW_BITS to decompress a raw stream,
    // instead of a gz stream with header.
    // miniz doesn't parse gz headers correctly
    err = mz_inflateInit2(&zstr_, -MZ_DEFAULT_WINDOW_BITS);

    if (err == MZ_OK) {
        log_i("zlib init ok");
        return true;
    } else {
        log_i("zlib error: %d", err);
        return false;
    }
}

auto GUnzip::decompress(uint8_t *input, int inputSize, uint8_t *output, int outputSize,
                        int &inputRead, int &outputAvail) -> int {

    //   if we are into the footer section, skip input bytes
    inputRead = 0;
    outputAvail = 0;
    if (compressedPos_ >= (compressedSize_ - 8)) {
        inputRead = inputSize;
        inputSize = 0;
    }

    if (!init_) {
        if (!init()) {
            return false;
        }
    }

    if (!readHeader_) {
        auto *hdr = (uint8_t *)&gzhdr;
        while (gzHeaderBytes_ < 10) {
            if (inputSize == 0) {
                // still reading header, but no more data
                // keep reading next call
                return MZ_OK;
            }

            hdr[gzHeaderBytes_] = *input;
            input++;
            inputSize--;
            inputRead++;
            gzHeaderBytes_++;
        }

        if (inputSize == 0) {
            return MZ_OK;
        }

        // check for filename in the gz header
        if ((gzhdr.fileFlags & GZ_FILE_HEADER_MASK) != 0) {
            while (!readHeader_) {
                if (*input == 0x00) {
                    readHeader_ = true;
                }

                input++;
                inputSize--;
                inputRead++;

                if (inputSize == 0) {
                    // keep reading filename next call
                    return MZ_OK;
                }
            }
        } else {
            readHeader_ = true;
        }
    }

    if (inputSize > 0) {
        int newPos = compressedPos_ + inputRead + inputSize;
        int footer = 0;
        if (newPos > compressedSize_ - 8) { // skip footer
            footer = newPos - (compressedSize_ - 8);
            inputSize -= footer;
            inputRead += footer;
        }
    }

    zstr_.next_in = input;
    zstr_.avail_in = inputSize;
    zstr_.next_out = output;
    zstr_.avail_out = outputSize;

    int inBefore = zstr_.total_in;
    int outBefore = zstr_.total_out;

    int err = MZ_OK;
    err = mz_inflate(&zstr_, MZ_SYNC_FLUSH); // NOLINT : prevent changing define

    inputRead += zstr_.total_in - inBefore;
    outputAvail = zstr_.total_out - outBefore;

    compressedPos_ += inputRead;
    uncompressedPos_ += outputAvail;

    return err;
}

auto GUnzip::decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                            const std::string &outputFileName) -> bool {

    SFile in = fileSystem.open(inputFileName);
    SFile out = fileSystem.open(outputFileName, "w", true);

    compressedSize_ = in.size();

    bool done = false;
    uint8_t inBuf[BSIZE];
    uint8_t outBuf[BSIZE];
    int inputAvail = 0;
    int inputIndex = 0;
    int inputRead = 0;
    int outputAvail = 0;
    bool last = false;
    while (!done) {
        if (inputAvail == 0) {

            if (last) {
                done = true;
                continue;
            }

            int readBytes = in.readBytes((char *)inBuf, BSIZE);
            if (readBytes == 0) {
                log_i("done reading!");
                done = true;
                continue;
            }

            if (readBytes < BSIZE) {
                last = true;
            }

            inputAvail = readBytes;
            inputIndex = 0;
        }

        int err = decompress(&inBuf[inputIndex], inputAvail, outBuf, BSIZE, inputRead, outputAvail);
        if (err != MZ_OK && err != MZ_STREAM_END) {
            log_e("Error decompressing %d", err);
            return false;
        }

        bool lastOutput = false;
        while (outputAvail > 0) {
            out.write(outBuf, outputAvail);
            out.flush();

            outputAvail = 0;

            if (lastOutput) {
                break;
            }

            int ir = 0;
            err = decompress(inBuf, 0, outBuf, BSIZE, ir, outputAvail);

            if (err == MZ_BUF_ERROR) {
                lastOutput = true;
                continue;
            }

            if (err != MZ_OK && err != MZ_STREAM_END) {
                log_e("Error decompressing %d", err);
                return false;
            }
        }

        // increment input
        inputIndex += inputRead;
        inputAvail -= inputRead;
    }

    log_i("done, closing.");
    in.close();
    out.close();

    return true;
}

auto GUnzip::decompressMemory(uint8_t *input, int inputSize) -> bool {

    uint8_t *fileptr = input;
    int filelen = inputSize;
    int currFileByte = 0;

    bool done = false;
    uint8_t inBuf[1024];
    uint8_t outBuf[1024];
    int inputAvail = 0;
    int inputIndex = 0;
    int inputRead = 0;
    int outputAvail = 0;
    bool last = false;
    while (!done) {

        if (inputAvail == 0) {

            if (last) {
                done = true;
                continue;
            }

            int bytesToRead = 1024;
            if ((filelen - currFileByte) < 1024) {
                bytesToRead = filelen - currFileByte;
            }

            memcpy(inBuf, fileptr, bytesToRead);
            fileptr += bytesToRead;
            currFileByte += bytesToRead;
            int readBytes = bytesToRead;

            if (readBytes == 0) {
                log_i("done reading!");
                done = true;
                continue;
            }

            if (readBytes < 1024) {
                last = true;
            }

            inputAvail = readBytes;
            inputIndex = 0;
        }

        int err = decompress(&inBuf[inputIndex], inputAvail, outBuf, 1024, inputRead, outputAvail);
        if (err != MZ_OK && err != MZ_STREAM_END) {
            log_e("Error decompressing %d", err);
        }

        // continue reading until no more output available
        while (outputAvail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, outputAvail);
            if (err != MZ_OK && err != MZ_STREAM_END) {
                log_e("error decompressing: %d", err);
                return false;
            }
        }

        // increment input
        inputIndex += inputRead;
        inputAvail -= inputRead;
    }

    log_i("done, closing.");
    return true;
}