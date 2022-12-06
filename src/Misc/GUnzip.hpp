#pragma once

#include "zlib.h"
#include <stdint.h>

class GUnzip {
public:
    GUnzip() {}
    ~GUnzip() {}

    void open(const char* fname);


protected:
    z_stream zstr_;
    uint8_t inflateBuf_[32768+7168];

};
