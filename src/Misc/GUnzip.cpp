#include "GUnzip.hpp"

#include <Arduino.h>
#include <cstdio>
#include <memory>

extern "C" {
#ifndef ZLIB_INTERNAL
#define ZLIB_INTERNAL
#endif
#include "Misc/unzipLIB/inftrees.h"
}

extern "C" {
#include "Misc/unzipLIB/inflate.h"
}

auto GUnzip::init() -> bool {

    inflateBuf_ = std::make_unique<uint8_t[]>(32768 + 7168);

    int err = Z_OK;

    zstr_.zalloc = (alloc_func) nullptr;
    zstr_.zfree = (free_func) nullptr;
    zstr_.opaque = (voidpf) nullptr;
    zstr_.state = (struct internal_state *)&inflateBuf_[32768];
    InflateState *st = (InflateState *)zstr_.state;
    st->window = inflateBuf_.get();

    zstr_.availIn = 0;
    zstr_.nextIn = Z_NULL;

    err = inflateInit2(&zstr_, 16 + MAX_WBITS);

    if (err == Z_OK) {
        log_i("zlib init ok\n");
        return true;
    } else {
        log_i("zlib error: %d\n", err);
        return false;
    }
}

auto GUnzip::decompress(uint8_t *input, int inputSize, uint8_t *output, int outputSize,
                        int &inputRead, int &outputAvail) -> int {

    zstr_.nextIn = input;
    zstr_.availIn = inputSize;
    zstr_.nextOut = output;
    zstr_.availOut = outputSize;

    int inBefore = zstr_.totalIn;
    int outBefore = zstr_.totalOut;

    int err = Z_OK;
    err = Inflate(&zstr_, Z_SYNC_FLUSH);

    inputRead = zstr_.totalIn - inBefore;
    outputAvail = zstr_.totalOut - outBefore;
    return err;
}

auto GUnzip::decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                            const std::string &outputFileName) -> bool {

    File in = fileSystem.open(inputFileName);
    File out = fileSystem.open(outputFileName, "w", true);

    if (!inflateBuf_) {
        if (!init()) {
            return false;
        }
    }

    bool done = false;
    uint8_t inBuf[1024];
    uint8_t outBuf[1024];
    int inputAvail = 0;
    int inputIndex = 0;
    int inputRead = 0;
    int outputAvail = 0;
    int block = 0;
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

        // write block
        if (outputAvail > 0) {
            out.write(outBuf, outputAvail);
        }

        // continue reading until no more output available
        while (outputAvail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, outputAvail);

            // write block
            if (outputAvail > 0) {
                out.write(outBuf, outputAvail);
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

    if (!inflateBuf_) {
        if (!init()) {
            return false;
        }
    }

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
    int block = 0;
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

        // continue reading until no more output available
        while (outputAvail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, outputAvail);
        }

        // increment input
        inputIndex += inputRead;
        inputAvail -= inputRead;
    }

    log_i("done, closing.");
    return true;
}