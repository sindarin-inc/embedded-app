#pragma once

#include <cinttypes>

#include "config.h"

#if TTF_SUPPORT
#include <unordered_map>
#endif

struct FontEntry {
    const char *caption;
    const uint8_t *content;
    int length;
#if TTF_SUPPORT
    const std::unordered_map<uint32_t, int16_t> *kerns;
    const std::unordered_map<uint32_t, uint16_t> *ligatures;
#endif
};
