#pragma once

#include <stdint.h>

#include "Storage/FileSystem.hpp"
#include "zlib.h"

class GUnzip {
public:
    GUnzip() {}
    ~GUnzip() {}

    void open(const char *fname);
    void decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                        const std::string &outputFileName);

    int decompress(uint8_t *input, int input_size, uint8_t *output, int output_size,
                   int &input_read, int &output_avail);

protected:
    z_stream zstr_;
    uint8_t inflateBuf_[32768 + 7168];
};
