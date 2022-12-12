#include "GUnzip.hpp"

#include <Arduino.h>
#include <memory>
#include <stdio.h>

extern "C" {
#ifndef ZLIB_INTERNAL
#define ZLIB_INTERNAL
#endif
#include "inftrees.h"
}

extern "C" {
#include "inflate.h"
}

bool GUnzip::init() {

    auto buffer = std::make_unique<uint8_t[]>(32768 + 7168);
    inflateBuf_ = buffer.get();

    int err = Z_OK;

    zstr_.zalloc = (alloc_func)0;
    zstr_.zfree = (free_func)0;
    zstr_.opaque = (voidpf)0;
    zstr_.state = (struct internal_state *)&inflateBuf_[32768];
    inflate_state *st = (inflate_state *)zstr_.state;
    st->window = inflateBuf_;

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

int GUnzip::decompress(uint8_t *input, int input_size, uint8_t *output, int output_size,
                       int &input_read, int &output_avail) {

    zstr_.next_in = input;
    zstr_.avail_in = input_size;
    zstr_.next_out = output;
    zstr_.avail_out = output_size;

    int in_before = zstr_.total_in;
    int out_before = zstr_.total_out;

    int err = Z_OK;
    err = inflate(&zstr_, Z_SYNC_FLUSH);

    input_read = zstr_.total_in - in_before;
    output_avail = zstr_.total_out - out_before;
    return err;
}

bool GUnzip::decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                            const std::string &outputFileName) {

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
    int input_avail = 0;
    int input_index = 0;
    int input_read = 0;
    int output_avail = 0;
    int block = 0;
    bool last = false;
    while (!done) {

        if (input_avail == 0) {

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

            input_avail = readBytes;
            input_index = 0;
        }

        int err =
            decompress(&inBuf[input_index], input_avail, outBuf, 1024, input_read, output_avail);

        // write block
        if (output_avail > 0) {
            out.write(outBuf, output_avail);
        }

        // continue reading until no more output available
        while (output_avail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, output_avail);

            // write block
            if (output_avail > 0) {
                out.write(outBuf, output_avail);
            }
        }

        // increment input
        input_index += input_read;
        input_avail -= input_read;
    }

    log_i("done, closing.");
    in.close();
    out.close();

    return true;
}

bool GUnzip::decompressMemory(uint8_t *input, int input_size) {

    if (!inflateBuf_) {
        if (!init()) {
            return false;
        }
    }

    uint8_t *fileptr = input;
    int filelen = input_size;
    int curr_file_byte = 0;

    bool done = false;
    uint8_t inBuf[1024];
    uint8_t outBuf[1024];
    int input_avail = 0;
    int input_index = 0;
    int input_read = 0;
    int output_avail = 0;
    int block = 0;
    bool last = false;
    while (!done) {

        if (input_avail == 0) {

            if (last) {
                done = true;
                continue;
            }

            int bytes_to_read = 1024;
            if ((filelen - curr_file_byte) < 1024) {
                bytes_to_read = filelen - curr_file_byte;
            }

            memcpy(inBuf, fileptr, bytes_to_read);
            fileptr += bytes_to_read;
            curr_file_byte += bytes_to_read;
            int readBytes = bytes_to_read;

            if (readBytes == 0) {
                log_i("done reading!");
                done = true;
                continue;
            }

            if (readBytes < 1024) {
                last = true;
            }

            input_avail = readBytes;
            input_index = 0;
        }

        int err =
            decompress(&inBuf[input_index], input_avail, outBuf, 1024, input_read, output_avail);

        // continue reading until no more output available
        while (output_avail == 1024) {
            int ir = 0;
            err = decompress(inBuf, 0, outBuf, 1024, ir, output_avail);
        }

        // increment input
        input_index += input_read;
        input_avail -= input_read;
    }

    log_i("done, closing.");
    return true;
}