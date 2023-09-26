#include "GUnzip.hpp"

#include <cstdio>
#include <memory>

#include "sindarin-debug.h"

//#define ZLIB_WINDOW_SIZE 32768
//#define ZLIB_INFLATE_STATE_SIZE 7168

typedef struct __attribute__((__packed__)) {
    uint8_t magic[2];
    uint8_t compressionMethod;
    uint8_t fileFlags;
    uint32_t timestamp;
    uint8_t compressionFlags;
    uint8_t operatingSystemID;
} gzHeader;

static gzHeader gzhdr;

static void *spiZAlloc(void *opaque, size_t items, size_t size) {
    log_d("spiZAlloc: %d %d", (int)items, (int)size);
    return SpiRamMalloc(items * size);
}
static void spiZFree(void *opaque, void *address) {
    log_d("spiZFree");
    SpiRamFree(address);
}

auto GUnzip::init() -> bool {

    int err = MZ_OK;
    init_ = true;
    compressedPos_ = 0;
    uncompressedPos_ = 0;

    memset(&zstr_, 0, sizeof(mz_stream));
    memset(&gzhdr, 0, sizeof(gzHeader));
    zstr_.zalloc = spiZAlloc;
    zstr_.zfree = spiZFree;

    zstr_.avail_in = 0;
    zstr_.next_in = 0;

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

    // printf("dec %d\n", inputSize);
    //  if we are into the footer section, skip
    if (compressedPos_ >= (compressedSize_ - 8)) {
        inputRead = inputSize;
        outputAvail = 0;
        return MZ_STREAM_END;
    }

    inputRead = 0;
    if (!init_) {
        if (!init()) {
            return false;
        }
    }

    if (gzhdr.magic[0] == 0x00) {
        printf("reading header\n");
        memcpy(&gzhdr, input, sizeof(gzHeader));
        printf("%x %x\n", gzhdr.magic[0], gzhdr.magic[1]);
        input += sizeof(gzHeader);
        inputSize -= sizeof(gzHeader);
        inputRead += sizeof(gzHeader);

        // check for filename
        if ((gzhdr.fileFlags & 0x08) != 0) {
            // skip filename
            while (*input != 0x00) {
                input++;
                inputSize--;
                inputRead++;
            }

            // skip one more byte
            input++;
            inputSize--;
            inputRead++;
        }
    }

    int newPos = compressedPos_ + inputRead + inputSize;
    int footer = 0;
    if (newPos > compressedSize_ - 8) { // skip footer
        footer = newPos - (compressedSize_ - 8);
        inputSize -= footer;
        inputRead += footer;
    }

    zstr_.next_in = input;
    zstr_.avail_in = inputSize;
    zstr_.next_out = output;
    zstr_.avail_out = outputSize;

    int inBefore = zstr_.total_in;
    int outBefore = zstr_.total_out;

    int err = MZ_OK;
    err = mz_inflate(&zstr_, MZ_SYNC_FLUSH);

    inputRead += zstr_.total_in - inBefore;
    outputAvail = zstr_.total_out - outBefore;

    // printf("dec read %d avail %d\n", inputRead, outputAvail);
    if (outputAvail > 0) {
        printf("upos %d\n", uncompressedPos_);
        for (int i = 0; i < outputAvail; i++) {
            printf("%c", output[i]);
        }
        printf("***\n");
    }

    compressedPos_ += inputRead;
    uncompressedPos_ += outputAvail;

    // printf("cpos %d upos %d\n", compressedPos_, uncompressedPos_);

    return err;
}

auto GUnzip::decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                            const std::string &outputFileName) -> bool {

    SFile in = fileSystem.open(inputFileName);
    SFile out = fileSystem.open(outputFileName, "w", true);

    compressedSize_ = in.size();

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

            int readBytes = in.readBytes((char *)inBuf, 1024);
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
            return false;
        }

        while (outputAvail > 0) {
            out.write(outBuf, outputAvail);
            if (outputAvail < 1024) {
                break;
            }

            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, outputAvail);
            if (outputAvail == 0 && err == MZ_BUF_ERROR) {
                break;
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