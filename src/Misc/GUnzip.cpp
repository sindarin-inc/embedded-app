#include "GUnzip.hpp"

#include <Arduino.h>
#include <cstdio>
#include <memory>

extern "C" {
#ifndef ZLIB_INTERNAL
#define ZLIB_INTERNAL
#endif
#include <inftrees.h>
}

extern "C" {
#include <inflate.h>
}

auto GUnzip::init() -> bool {

    inflateBuf_ = std::make_unique<uint8_t[]>(32768 + 7168);

    int err = Z_OK;

    zstr_.zalloc = (alloc_func) nullptr;
    zstr_.zfree = (free_func) nullptr;
    zstr_.opaque = (voidpf) nullptr;
    zstr_.state = (struct internal_state *)&inflateBuf_[32768];
    auto *st = (inflate_state *)zstr_.state;
    st->window = inflateBuf_.get();

    zstr_.avail_in = 0;
    zstr_.next_in = Z_NULL;

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

    zstr_.next_in = input;
    zstr_.avail_in = inputSize;
    zstr_.next_out = output;
    zstr_.avail_out = outputSize;

    int inBefore = zstr_.total_in;
    int outBefore = zstr_.total_out;

    int err = Z_OK;
    err = inflate(&zstr_, Z_SYNC_FLUSH);

    inputRead = zstr_.total_in - inBefore;
    outputAvail = zstr_.total_out - outBefore;
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
        if (err != Z_OK && err != Z_STREAM_END) {
            log_e("Error decompressing %d", err);
            return false;
        }

        // write block
        if (outputAvail > 0) {
            out.write(outBuf, outputAvail);
        }

        // continue reading until no more output available
        while (outputAvail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, outputAvail);
            if (err != Z_OK && err != Z_STREAM_END) {
                log_e("Error decompressing %d", err);
                return false;
            }

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
        if (err != Z_OK && err != Z_STREAM_END) {
            log_e("Error decompressing %d", err);
        }

        // continue reading until no more output available
        while (outputAvail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, outputAvail);
            if (err != Z_OK && err != Z_STREAM_END) {
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