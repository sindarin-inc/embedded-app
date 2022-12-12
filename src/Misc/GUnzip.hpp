#pragma once

#include <stdint.h>

#include "Misc/unzipLIB/zlib.h"
#include "Storage/FileSystem.hpp"

class GUnzip {
public:
    GUnzip() : inflateBuf_(NULL) {}
    ~GUnzip() {}

    bool decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                        const std::string &outputFileName);

    bool decompressMemory(uint8_t *input, int input_size);

protected:
    z_stream zstr_;
    uint8_t *inflateBuf_;

    bool init();
    int decompress(uint8_t *input, int input_size, uint8_t *output, int output_size,
                   int &input_read, int &output_avail);
};
