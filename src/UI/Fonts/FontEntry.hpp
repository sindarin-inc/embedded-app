#pragma once

#include <cinttypes>

#include "config.h"

struct FontEntry {
    const char *caption;
    const uint8_t *content;
    int length;
};
